#include "mqtt_client.hpp"
#include "logger.hpp"
#include <cstring>
#include <iostream>

namespace industrial {

MqttClient::MqttClient(const std::string& broker_host, int broker_port,
                       const std::string& client_id, const std::string& topic_prefix)
    : broker_host_(broker_host),
      broker_port_(broker_port),
      client_id_(client_id),
      topic_prefix_(topic_prefix) {
    init_network_subsystem();
}

MqttClient::~MqttClient() {
    disconnect_broker();
}

void MqttClient::encode_remaining_length(size_t length, std::vector<uint8_t>& out) {
    do {
        uint8_t byte = length % 128;
        length /= 128;
        if (length > 0) {
            byte |= 0x80;
        }
        out.push_back(byte);
    } while (length > 0);
}

std::vector<uint8_t> MqttClient::build_connect_packet() {
    std::vector<uint8_t> var_and_payload;

    // Protocol Name: "MQTT"
    var_and_payload.push_back(0x00);
    var_and_payload.push_back(0x04);
    var_and_payload.push_back('M');
    var_and_payload.push_back('Q');
    var_and_payload.push_back('T');
    var_and_payload.push_back('T');

    // Protocol Level: 4 (MQTT 3.1.1)
    var_and_payload.push_back(0x04);

    // Connect Flags: Clean session (0x02)
    var_and_payload.push_back(0x02);

    // Keep Alive: 60 seconds (0x003C)
    var_and_payload.push_back(0x00);
    var_and_payload.push_back(0x3C);

    // Payload: Client Identifier
    uint16_t cid_len = static_cast<uint16_t>(client_id_.size());
    var_and_payload.push_back(static_cast<uint8_t>((cid_len >> 8) & 0xFF));
    var_and_payload.push_back(static_cast<uint8_t>(cid_len & 0xFF));
    var_and_payload.insert(var_and_payload.end(), client_id_.begin(), client_id_.end());

    // Fixed Header
    std::vector<uint8_t> packet;
    packet.push_back(0x10); // CONNECT packet type
    encode_remaining_length(var_and_payload.size(), packet);
    packet.insert(packet.end(), var_and_payload.begin(), var_and_payload.end());

    return packet;
}

std::vector<uint8_t> MqttClient::build_publish_packet(const std::string& topic, const std::string& payload) {
    std::vector<uint8_t> var_and_payload;

    // Topic Name
    uint16_t t_len = static_cast<uint16_t>(topic.size());
    var_and_payload.push_back(static_cast<uint8_t>((t_len >> 8) & 0xFF));
    var_and_payload.push_back(static_cast<uint8_t>(t_len & 0xFF));
    var_and_payload.insert(var_and_payload.end(), topic.begin(), topic.end());

    // Application Message (Payload)
    var_and_payload.insert(var_and_payload.end(), payload.begin(), payload.end());

    // Fixed Header: PUBLISH, QoS 0
    std::vector<uint8_t> packet;
    packet.push_back(0x30);
    encode_remaining_length(var_and_payload.size(), packet);
    packet.insert(packet.end(), var_and_payload.begin(), var_and_payload.end());

    return packet;
}

bool MqttClient::connect_broker() {
    if (!enabled_.load()) return false;
    if (connected_.load()) return true;

    std::lock_guard<std::mutex> lock(send_mutex_);

    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET_FD) {
        return false;
    }

    sockaddr_in broker_addr{};
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(static_cast<uint16_t>(broker_port_));
    inet_pton(AF_INET, broker_host_.c_str(), &broker_addr.sin_addr);

    if (connect(sock_, (struct sockaddr*)&broker_addr, sizeof(broker_addr)) == SOCKET_ERROR_VAL) {
        close_socket(sock_);
        sock_ = INVALID_SOCKET_FD;
        return false;
    }

    auto connect_pkt = build_connect_packet();
#if defined(_WIN32) || defined(_WIN64)
    int sent = send(sock_, (const char*)connect_pkt.data(), static_cast<int>(connect_pkt.size()), 0);
#else
    ssize_t sent = send(sock_, connect_pkt.data(), connect_pkt.size(), MSG_NOSIGNAL);
#endif

    if (sent <= 0) {
        close_socket(sock_);
        sock_ = INVALID_SOCKET_FD;
        return false;
    }

    // Read CONNACK (expecting 4 bytes: 0x20 0x02 0x00 0x00)
    uint8_t connack[4];
#if defined(_WIN32) || defined(_WIN64)
    int recvd = recv(sock_, (char*)connack, 4, 0);
#else
    ssize_t recvd = recv(sock_, connack, 4, 0);
#endif

    if (recvd >= 4 && connack[0] == 0x20 && connack[3] == 0x00) {
        connected_.store(true);
        LOG_INFO("MQTT broker connected successfully (" + broker_host_ + ":" + std::to_string(broker_port_) + ")");
        return true;
    }

    close_socket(sock_);
    sock_ = INVALID_SOCKET_FD;
    return false;
}

void MqttClient::disconnect_broker() {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (sock_ != INVALID_SOCKET_FD) {
        if (connected_.load()) {
            uint8_t disconn_pkt[2] = {0xE0, 0x00};
#if defined(_WIN32) || defined(_WIN64)
            send(sock_, (const char*)disconn_pkt, 2, 0);
#else
            send(sock_, disconn_pkt, 2, MSG_NOSIGNAL);
#endif
        }
        close_socket(sock_);
        sock_ = INVALID_SOCKET_FD;
    }
    connected_.store(false);
}

bool MqttClient::publish(const std::string& subtopic, const std::string& payload) {
    std::string full_topic = topic_prefix_ + "/" + subtopic;
    return publish_raw(full_topic, payload);
}

bool MqttClient::publish_raw(const std::string& full_topic, const std::string& payload) {
    if (!enabled_.load() || !connected_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(send_mutex_);
    auto pub_pkt = build_publish_packet(full_topic, payload);

#if defined(_WIN32) || defined(_WIN64)
    int sent = send(sock_, (const char*)pub_pkt.data(), static_cast<int>(pub_pkt.size()), 0);
#else
    ssize_t sent = send(sock_, pub_pkt.data(), pub_pkt.size(), MSG_NOSIGNAL);
#endif

    if (sent <= 0) {
        connected_.store(false);
        close_socket(sock_);
        sock_ = INVALID_SOCKET_FD;
        return false;
    }
    return true;
}

} // namespace industrial

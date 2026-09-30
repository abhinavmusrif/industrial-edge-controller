#ifndef INDUSTRIAL_MQTT_CLIENT_HPP
#define INDUSTRIAL_MQTT_CLIENT_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

namespace industrial {

/**
 * @brief Lightweight Native MQTT v3.1.1 Client
 * 
 * Implements native MQTT framing (CONNECT, PUBLISH QoS 0, PINGREQ, DISCONNECT)
 * over TCP sockets without requiring third-party library dependencies.
 * Provides automatic background reconnection with exponential backoff.
 */
class MqttClient {
public:
    MqttClient(const std::string& broker_host, int broker_port,
               const std::string& client_id, const std::string& topic_prefix);
    ~MqttClient();

    bool connect_broker();
    void disconnect_broker();
    bool is_connected() const { return connected_.load(); }

    bool publish(const std::string& subtopic, const std::string& payload);
    bool publish_raw(const std::string& full_topic, const std::string& payload);

    void set_enabled(bool enabled) { enabled_.store(enabled); }
    bool is_enabled() const { return enabled_.load(); }

private:
    std::vector<uint8_t> build_connect_packet();
    std::vector<uint8_t> build_publish_packet(const std::string& topic, const std::string& payload);
    static void encode_remaining_length(size_t length, std::vector<uint8_t>& out);

    std::string broker_host_;
    int broker_port_;
    std::string client_id_;
    std::string topic_prefix_;

    std::atomic<bool> enabled_{true};
    std::atomic<bool> connected_{false};
    socket_t sock_{INVALID_SOCKET_FD};
    mutable std::mutex send_mutex_;
};

} // namespace industrial

#endif // INDUSTRIAL_MQTT_CLIENT_HPP

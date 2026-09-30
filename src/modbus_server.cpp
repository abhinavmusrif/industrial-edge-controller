#include "modbus_server.hpp"
#include "gateway.hpp"
#include "logger.hpp"
#include <iostream>
#include <cstring>

namespace industrial {

ModbusServer::ModbusServer(const std::string& bind_addr, int port, uint8_t unit_id, LinuxGateway& gateway)
    : bind_addr_(bind_addr), port_(port), unit_id_(unit_id), gateway_(gateway) {
    init_network_subsystem();
}

ModbusServer::~ModbusServer() {
    stop();
}

bool ModbusServer::start() {
    if (running_.load()) return true;

    listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock_ == INVALID_SOCKET_FD) {
        LOG_ERROR("Failed to create Modbus TCP socket");
        return false;
    }

    int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, bind_addr_.c_str(), &addr.sin_addr);

    if (bind(listen_sock_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_VAL) {
        LOG_ERROR("Failed to bind Modbus TCP server on " + bind_addr_ + ":" + std::to_string(port_));
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCKET_FD;
        return false;
    }

    if (listen(listen_sock_, 10) == SOCKET_ERROR_VAL) {
        LOG_ERROR("Failed to listen on Modbus socket");
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCKET_FD;
        return false;
    }

    running_.store(true);
    server_thread_ = std::thread(&ModbusServer::accept_loop, this);
    LOG_INFO("Modbus TCP Server listening on " + bind_addr_ + ":" + std::to_string(port_));
    return true;
}

void ModbusServer::stop() {
    if (!running_.load()) return;
    running_.store(false);

    if (listen_sock_ != INVALID_SOCKET_FD) {
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCKET_FD;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::lock_guard<std::mutex> lock(client_mutex_);
    for (auto& th : client_threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
    client_threads_.clear();
}

void ModbusServer::accept_loop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
#if defined(_WIN32) || defined(_WIN64)
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif

        socket_t client_sock = accept(listen_sock_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock == INVALID_SOCKET_FD) {
            if (!running_.load()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::lock_guard<std::mutex> lock(client_mutex_);
        client_threads_.emplace_back(&ModbusServer::handle_client, this, client_sock);
    }
}

void ModbusServer::handle_client(socket_t client_sock) {
    uint8_t buffer[260];

    while (running_.load()) {
#if defined(_WIN32) || defined(_WIN64)
        int bytes = recv(client_sock, (char*)buffer, sizeof(buffer), 0);
#else
        ssize_t bytes = recv(client_sock, buffer, sizeof(buffer), 0);
#endif
        if (bytes <= 0) break;

        if (bytes >= 12) { // Minimum Modbus TCP read request
            auto resp = handle_modbus_request(buffer, bytes);
            if (!resp.empty()) {
#if defined(_WIN32) || defined(_WIN64)
                send(client_sock, (const char*)resp.data(), static_cast<int>(resp.size()), 0);
#else
                send(client_sock, resp.data(), resp.size(), MSG_NOSIGNAL);
#endif
            }
        }
    }

    close_socket(client_sock);
}

std::vector<uint8_t> ModbusServer::handle_modbus_request(const uint8_t* req, size_t len) {
    std::vector<uint8_t> resp;
    if (len < 12) return resp;

    uint16_t proto_id = (req[2] << 8) | req[3];
    uint8_t  unit_id  = req[6];
    uint8_t  func_code= req[7];

    if (proto_id != 0) return resp; // Not standard Modbus TCP

    // Only process Read Holding Registers (0x03)
    if (func_code != 0x03) {
        // Exception response: Function code with high bit set, Exception code 0x01 (Illegal Function)
        resp.push_back(req[0]); resp.push_back(req[1]); // Trans ID
        resp.push_back(0); resp.push_back(0);           // Proto ID
        resp.push_back(0); resp.push_back(3);           // Length = 3
        resp.push_back(unit_id);
        resp.push_back(func_code | 0x80);
        resp.push_back(0x01);                           // Illegal Function
        return resp;
    }

    uint16_t start_addr = (req[8] << 8) | req[9];
    uint16_t quantity   = (req[10] << 8) | req[11];

    if (quantity == 0 || quantity > 125 || start_addr > 7 || (start_addr + quantity) > 8) {
        // Exception 0x02: Illegal Data Address
        resp.push_back(req[0]); resp.push_back(req[1]);
        resp.push_back(0); resp.push_back(0);
        resp.push_back(0); resp.push_back(3);
        resp.push_back(unit_id);
        resp.push_back(func_code | 0x80);
        resp.push_back(0x02); // Illegal Data Address
        return resp;
    }

    // Build registers 0..7
    // 0: Temp x 10
    // 1: Current x 10
    // 2: Vibration x 10
    // 3: RPM
    // 4: Controller State
    // 5: Fault Code
    // 6: Watchdog Status
    // 7: MCU Status
    auto t = gateway_.get_current_telemetry();
    uint16_t registers[8];
    registers[0] = static_cast<uint16_t>(static_cast<int16_t>(t.temperature * 10.0f));
    registers[1] = static_cast<uint16_t>(static_cast<int16_t>(t.current * 10.0f));
    registers[2] = static_cast<uint16_t>(static_cast<int16_t>(t.vibration * 10.0f));
    registers[3] = static_cast<uint16_t>(t.rpm);
    registers[4] = static_cast<uint16_t>(gateway_.get_actuator_state());
    registers[5] = static_cast<uint16_t>(gateway_.get_fault_manager().get_highest_severity_fault());
    registers[6] = t.watchdog_ok ? 1 : 0;
    registers[7] = t.mcu_connected ? 1 : 0;

    uint8_t byte_count = static_cast<uint8_t>(quantity * 2);
    uint16_t resp_len = 1 + 1 + 1 + byte_count; // UnitId + FuncCode + ByteCount + Data

    // MBAP Header
    resp.push_back(req[0]); resp.push_back(req[1]); // Trans ID
    resp.push_back(0); resp.push_back(0);           // Proto ID
    resp.push_back(static_cast<uint8_t>((resp_len >> 8) & 0xFF));
    resp.push_back(static_cast<uint8_t>(resp_len & 0xFF));
    resp.push_back(unit_id);

    // PDU Response
    resp.push_back(func_code);
    resp.push_back(byte_count);
    for (uint16_t i = 0; i < quantity; ++i) {
        uint16_t val = registers[start_addr + i];
        resp.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        resp.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    return resp;
}

} // namespace industrial

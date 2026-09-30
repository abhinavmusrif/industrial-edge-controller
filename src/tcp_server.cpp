#include "tcp_server.hpp"
#include "gateway.hpp"
#include "logger.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace industrial {

CliMonitorServer::CliMonitorServer(const std::string& host, int port, LinuxGateway& gateway)
    : host_(host), port_(port), gateway_(gateway) {
    init_network_subsystem();
}

CliMonitorServer::~CliMonitorServer() {
    stop();
}

bool CliMonitorServer::start() {
    if (running_.load()) return true;

    listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock_ == INVALID_SOCKET_FD) {
        LOG_ERROR("Failed to create CLI monitor socket");
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
    inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

    if (bind(listen_sock_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_VAL) {
        LOG_ERROR("Failed to bind CLI monitor server on " + host_ + ":" + std::to_string(port_));
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCKET_FD;
        return false;
    }

    if (listen(listen_sock_, 5) == SOCKET_ERROR_VAL) {
        LOG_ERROR("Failed to listen on CLI monitor socket");
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCKET_FD;
        return false;
    }

    running_.store(true);
    server_thread_ = std::thread(&CliMonitorServer::accept_loop, this);
    LOG_INFO("CLI Monitoring server listening on " + host_ + ":" + std::to_string(port_));
    return true;
}

void CliMonitorServer::stop() {
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

void CliMonitorServer::accept_loop() {
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
        client_threads_.emplace_back(&CliMonitorServer::handle_client, this, client_sock);
    }
}

void CliMonitorServer::handle_client(socket_t client_sock) {
    std::string banner =
        "\r\n=======================================================\r\n"
        " Industrial Edge Controller CLI Monitor (Digital Twin)\r\n"
        " Commands: STATUS, SENSORS, FAULTS, ACTUATOR, WATCHDOG,\r\n"
        "           JSON, START, STOP, RESET, ESTOP, HELP, QUIT\r\n"
        "=======================================================\r\n\r\n> ";

#if defined(_WIN32) || defined(_WIN64)
    send(client_sock, banner.c_str(), static_cast<int>(banner.size()), 0);
#else
    send(client_sock, banner.c_str(), banner.size(), MSG_NOSIGNAL);
#endif

    std::string cmd_buffer;
    char buf[256];

    while (running_.load()) {
#if defined(_WIN32) || defined(_WIN64)
        int n = recv(client_sock, buf, sizeof(buf) - 1, 0);
#else
        ssize_t n = recv(client_sock, buf, sizeof(buf) - 1, 0);
#endif
        if (n <= 0) break;

        for (int i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\r' || c == '\n') {
                if (!cmd_buffer.empty()) {
                    std::string resp = process_command(cmd_buffer);
                    if (cmd_buffer == "QUIT" || cmd_buffer == "EXIT") {
#if defined(_WIN32) || defined(_WIN64)
                        send(client_sock, resp.c_str(), static_cast<int>(resp.size()), 0);
#else
                        send(client_sock, resp.c_str(), resp.size(), MSG_NOSIGNAL);
#endif
                        close_socket(client_sock);
                        return;
                    }
                    resp += "\r\n> ";
#if defined(_WIN32) || defined(_WIN64)
                    send(client_sock, resp.c_str(), static_cast<int>(resp.size()), 0);
#else
                    send(client_sock, resp.c_str(), resp.size(), MSG_NOSIGNAL);
#endif
                    cmd_buffer.clear();
                }
            } else {
                cmd_buffer += c;
            }
        }
    }

    close_socket(client_sock);
}

std::string CliMonitorServer::process_command(const std::string& raw_cmd) {
    std::string cmd = raw_cmd;
    // Trim
    while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\t')) cmd.pop_back();
    while (!cmd.empty() && (cmd.front() == ' ' || cmd.front() == '\t')) cmd.erase(cmd.begin());
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "STATUS") {
        auto t = gateway_.get_current_telemetry();
        std::ostringstream oss;
        oss << "\r\nDEVICE:        " << t.device_id << "\r\n"
            << "MCU:           " << (t.mcu_connected ? "CONNECTED" : "DISCONNECTED") << "\r\n"
            << "CONTROLLER:    " << t.controller_state << "\r\n"
            << "TEMPERATURE:   " << std::fixed << std::setprecision(1) << t.temperature << " C\r\n"
            << "CURRENT:       " << std::fixed << std::setprecision(2) << t.current << " A\r\n"
            << "VIBRATION:     " << std::fixed << std::setprecision(2) << t.vibration << " mm/s\r\n"
            << "RPM:           " << std::fixed << std::setprecision(0) << t.rpm << "\r\n"
            << "WATCHDOG:      " << (t.watchdog_ok ? "OK" : "EXPIRED") << " (" << t.watchdog_elapsed_ms << " ms)\r\n"
            << "TRIP REASON:   " << t.last_trip_reason << "\r\n";
        return oss.str();
    }
    else if (cmd == "SENSORS") {
        auto sensors = gateway_.get_all_sensors();
        std::ostringstream oss;
        oss << "\r\nID  NAME           RAW      FILTERED  UNIT    WARNING  CRITICAL\r\n"
            << "---------------------------------------------------------------\r\n";
        for (const auto& [id, s] : sensors) {
            oss << std::setw(3) << static_cast<int>(id) << " "
                << std::setw(14) << std::left << sensor_id_to_string(id) << " "
                << std::setw(8) << std::fixed << std::setprecision(2) << s.raw_value << " "
                << std::setw(9) << s.filtered_value << " "
                << std::setw(7) << (id == SensorId::TEMPERATURE ? "C" : id == SensorId::CURRENT ? "A" : id == SensorId::VIBRATION ? "mm/s" : "RPM") << " "
                << std::setw(8) << (s.is_warning ? "YES" : "NO") << " "
                << (s.is_critical ? "YES" : "NO") << "\r\n";
        }
        return oss.str();
    }
    else if (cmd == "FAULTS") {
        auto faults = gateway_.get_fault_manager().get_active_faults();
        std::ostringstream oss;
        oss << "\r\n--- ACTIVE FAULTS (" << faults.size() << ") ---\r\n";
        if (faults.empty()) {
            oss << "None. All systems operational.\r\n";
        } else {
            for (const auto& f : faults) {
                oss << "[FAULT] " << f.description << "\r\n";
            }
        }
        return oss.str();
    }
    else if (cmd == "ACTUATOR") {
        std::ostringstream oss;
        oss << "\r\nACTUATOR STATE: " << gateway_.get_actuator_state_str() << "\r\n"
            << "TRIP REASON:    " << gateway_.get_controller().get_last_trip_reason() << "\r\n"
            << "HYSTERESIS:     " << gateway_.get_controller().get_hysteresis_ms() << " ms\r\n"
            << "Commands: START, STOP, RESET, ESTOP\r\n";
        return oss.str();
    }
    else if (cmd == "START") {
        gateway_.get_controller().command_start();
        return "Actuator START commanded.\r\n";
    }
    else if (cmd == "STOP") {
        gateway_.get_controller().command_stop();
        return "Actuator STOP commanded.\r\n";
    }
    else if (cmd == "RESET") {
        gateway_.get_controller().command_reset();
        gateway_.get_fault_manager().clear_all();
        return "Actuator RESET and faults cleared.\r\n";
    }
    else if (cmd == "ESTOP") {
        gateway_.get_controller().command_emergency_stop("CLI Manual Operator ESTOP");
        return "CRITICAL: EMERGENCY_STOP triggered by operator.\r\n";
    }
    else if (cmd == "WATCHDOG") {
        std::ostringstream oss;
        oss << "\r\nWATCHDOG HEALTH: " << (gateway_.is_watchdog_ok() ? "OK" : "EXPIRED/TRIPPED") << "\r\n"
            << "ELAPSED:         " << gateway_.get_watchdog_elapsed_ms() << " ms\r\n"
            << "TIMEOUT SETTING: " << gateway_.get_config().watchdog_timeout_ms << " ms\r\n";
        return oss.str();
    }
    else if (cmd == "JSON") {
        return "\r\n" + gateway_.get_current_telemetry().to_json() + "\r\n";
    }
    else if (cmd == "QUIT" || cmd == "EXIT") {
        return "Goodbye.\r\n";
    }
    else {
        return "\r\nAvailable Commands:\r\n"
               "  STATUS   - Complete device and sensor summary\r\n"
               "  SENSORS  - Detailed virtual sensor telemetry\r\n"
               "  FAULTS   - List active faults and alarms\r\n"
               "  ACTUATOR - Actuator state and trip diagnostics\r\n"
               "  START    - Start simulated actuator\r\n"
               "  STOP     - Stop simulated actuator\r\n"
               "  RESET    - Clear faults and reset trips\r\n"
               "  ESTOP    - Operator emergency stop\r\n"
               "  WATCHDOG - Watchdog timer status\r\n"
               "  JSON     - Output raw JSON telemetry\r\n"
               "  QUIT     - Disconnect session\r\n";
    }
}

} // namespace industrial

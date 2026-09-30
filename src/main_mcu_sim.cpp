#include "common.hpp"
#include "protocol.hpp"
#include "sensor_manager.hpp"
#include "transport.hpp"
#include "logger.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <csignal>
#include <sstream>

using namespace industrial;

static std::atomic<bool> g_mcu_running{true};

// Fault injection flags
static std::atomic<bool> g_fault_bad_crc{false};
static std::atomic<int>  g_fault_packet_loss_pct{0};
static std::atomic<int>  g_fault_network_delay_ms{0};
static std::atomic<bool> g_fault_pause_tx{false};
static std::atomic<uint64_t> g_fault_pause_until_ms{0};

static void sig_handler(int) {
    g_mcu_running.store(false);
}

// Background dynamic fault listener server on port 9001
void run_fault_server(int port, SensorManager& sm) {
    init_network_subsystem();
    socket_t listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET_FD) return;

    int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_VAL ||
        listen(listen_sock, 2) == SOCKET_ERROR_VAL) {
        close_socket(listen_sock);
        return;
    }

    while (g_mcu_running.load()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_sock, &fds);
        timeval tv{0, 200000}; // 200ms
        if (select(static_cast<int>(listen_sock) + 1, &fds, nullptr, nullptr, &tv) <= 0) {
            continue;
        }

        socket_t cs = accept(listen_sock, nullptr, nullptr);
        if (cs == INVALID_SOCKET_FD) continue;

        char buf[256];
#if defined(_WIN32) || defined(_WIN64)
        int n = recv(cs, buf, sizeof(buf) - 1, 0);
#else
        ssize_t n = recv(cs, buf, sizeof(buf) - 1, 0);
#endif
        if (n > 0) {
            buf[n] = '\0';
            std::string cmd(buf);
            // Trim
            while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == '\n' || cmd.back() == ' ')) cmd.pop_back();

            std::string resp = "OK\n";
            if (cmd.find("TEMP_HIGH") != std::string::npos) {
                sm.inject_sensor_fault(SensorId::TEMPERATURE, 92.5f);
                resp = "FAULT_INJECTED: TEMPERATURE=92.5C\n";
            } else if (cmd.find("CURRENT_HIGH") != std::string::npos) {
                sm.inject_sensor_fault(SensorId::CURRENT, 16.8f);
                resp = "FAULT_INJECTED: CURRENT=16.8A\n";
            } else if (cmd.find("VIBE_HIGH") != std::string::npos) {
                sm.inject_sensor_fault(SensorId::VIBRATION, 8.4f);
                resp = "FAULT_INJECTED: VIBRATION=8.4mm/s\n";
            } else if (cmd.find("BAD_CRC") != std::string::npos) {
                g_fault_bad_crc.store(true);
                resp = "FAULT_INJECTED: BAD_CRC enabled\n";
            } else if (cmd.find("DROP") != std::string::npos) {
                g_fault_packet_loss_pct.store(50);
                resp = "FAULT_INJECTED: PACKET_LOSS 50%\n";
            } else if (cmd.find("DELAY") != std::string::npos) {
                g_fault_network_delay_ms.store(300);
                resp = "FAULT_INJECTED: NETWORK_DELAY 300ms\n";
            } else if (cmd.find("PAUSE") != std::string::npos) {
                // Pause transmitting for 4 seconds to trigger gateway watchdog
                g_fault_pause_tx.store(true);
                g_fault_pause_until_ms.store(get_steady_time_ms() + 4000);
                resp = "FAULT_INJECTED: PAUSED TRANSMISSION for 4s (Watchdog test)\n";
            } else if (cmd.find("RESET") != std::string::npos || cmd.find("CLEAR") != std::string::npos) {
                sm.clear_all_faults();
                g_fault_bad_crc.store(false);
                g_fault_packet_loss_pct.store(0);
                g_fault_network_delay_ms.store(0);
                g_fault_pause_tx.store(false);
                resp = "FAULTS_CLEARED: All nominal\n";
            }
#if defined(_WIN32) || defined(_WIN64)
            send(cs, resp.c_str(), static_cast<int>(resp.size()), 0);
#else
            send(cs, resp.c_str(), resp.size(), MSG_NOSIGNAL);
#endif
        }
        close_socket(cs);
    }
    close_socket(listen_sock);
}

int main_mcu_sim(int argc, char* argv[]) {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    std::string gateway_host = "127.0.0.1";
    int gateway_port = 9000;
    int fault_port = 9001;

    // Check CLI arguments for static fault injections
    bool cli_temp_high = false;
    bool cli_curr_high = false;
    bool cli_vibe_high = false;
    bool cli_bad_crc = false;
    int cli_packet_loss = 0;
    int cli_network_delay = 0;
    bool cli_sensor_timeout = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--temperature-high") cli_temp_high = true;
        else if (arg == "--current-high") cli_curr_high = true;
        else if (arg == "--vibration-high") cli_vibe_high = true;
        else if (arg == "--bad-crc") cli_bad_crc = true;
        else if (arg == "--packet-loss") cli_packet_loss = 50;
        else if (arg == "--network-delay") cli_network_delay = 200;
        else if (arg == "--sensor-timeout") cli_sensor_timeout = true;
        else if (arg == "--port" && i + 1 < argc) gateway_port = std::stoi(argv[++i]);
        else if (arg == "--host" && i + 1 < argc) gateway_host = argv[++i];
    }

    std::cout << "==================================================\r\n";
    std::cout << "Starting MCU Simulator (Digital Twin Subsystem)\r\n";
    std::cout << "Connecting to Gateway on " << gateway_host << ":" << gateway_port << "\r\n";
    std::cout << "Fault Listener running on port " << fault_port << "\r\n";
    std::cout << "==================================================\r\n";

    SensorManager sensor_mgr;
    if (cli_temp_high) sensor_mgr.inject_sensor_fault(SensorId::TEMPERATURE, 94.0f);
    if (cli_curr_high) sensor_mgr.inject_sensor_fault(SensorId::CURRENT, 17.5f);
    if (cli_vibe_high) sensor_mgr.inject_sensor_fault(SensorId::VIBRATION, 8.5f);
    if (cli_bad_crc) g_fault_bad_crc.store(true);
    if (cli_packet_loss > 0) g_fault_packet_loss_pct.store(cli_packet_loss);
    if (cli_network_delay > 0) g_fault_network_delay_ms.store(cli_network_delay);
    if (cli_sensor_timeout) {
        g_fault_pause_tx.store(true);
        g_fault_pause_until_ms.store(get_steady_time_ms() + 60000); // 60s silence
    }

    // Launch background fault injection listener
    std::thread fault_th(run_fault_server, fault_port, std::ref(sensor_mgr));

    TcpTransport transport(gateway_host, gateway_port, false);

    // Connection retry loop
    while (g_mcu_running.load() && !transport.is_connected()) {
        std::cout << "[MCU] Attempting connection to Linux Gateway (" << gateway_host << ":" << gateway_port << ")...\n";
        if (transport.connect_endpoint()) {
            std::cout << "[MCU] Successfully connected to Linux Edge Gateway!\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::atomic<uint32_t> sequence{0};

    // Callback invoked when sensors are sampled
    sensor_mgr.set_sample_callback([&](const SensorReading& r) {
        if (!transport.is_connected() || !g_mcu_running.load()) return;

        // Check if transmission is paused (Watchdog test)
        if (g_fault_pause_tx.load()) {
            if (get_steady_time_ms() < g_fault_pause_until_ms.load()) {
                return; // Suppress frame transmission!
            }
            g_fault_pause_tx.store(false);
            std::cout << "[MCU] Resuming transmission after pause.\n";
        }

        // Check packet loss simulation
        int drop_pct = g_fault_packet_loss_pct.load();
        if (drop_pct > 0 && (rand() % 100) < drop_pct) {
            std::cout << "[MCU Fault] Packet dropped deliberately (simulating loss)\n";
            return;
        }

        // Check simulated network delay
        int delay_ms = g_fault_network_delay_ms.load();
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }

        // Construct status flags
        uint8_t flags = 0;
        if (r.is_warning) flags |= 0x01;
        if (r.is_critical) flags |= 0x02;

        Frame frame = Protocol::create_sensor_frame(
            1, ++sequence,
            static_cast<uint8_t>(r.id),
            r.raw_value,
            r.filtered_value,
            flags
        );

        bool corrupt = g_fault_bad_crc.load();
        auto wire_bytes = Protocol::serialize_frame(frame, corrupt);

        transport.send_bytes(wire_bytes.data(), wire_bytes.size());
    });

    sensor_mgr.start();

    // Main MCU loop: sends periodic heartbeats every 1000ms
    while (g_mcu_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if (!transport.is_connected()) {
            transport.connect_endpoint();
        } else if (!g_fault_pause_tx.load()) {
            Frame hb = Protocol::create_heartbeat_frame(1, ++sequence);
            auto hb_bytes = Protocol::serialize_frame(hb);
            transport.send_bytes(hb_bytes.data(), hb_bytes.size());
        }
    }

    std::cout << "[MCU] Shutting down MCU Simulator...\n";
    sensor_mgr.stop();
    transport.disconnect_endpoint();

    if (fault_th.joinable()) {
        fault_th.join();
    }

    std::cout << "[MCU] Finished cleanly.\n";
    return 0;
}

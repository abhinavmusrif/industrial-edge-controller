#include "gateway.hpp"
#include "logger.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

using namespace industrial;

static std::atomic<bool> g_gateway_running{true};

static void sig_handler(int) {
    g_gateway_running.store(false);
}

int main_gateway(int argc, char* argv[]) {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    std::string config_path = "config/gateway.conf";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    GatewayConfig config = GatewayConfig::load_from_file(config_path);
    LinuxGateway gateway(config);

    if (!gateway.start()) {
        std::cerr << "Failed to start Linux Edge Gateway!" << std::endl;
        return 1;
    }

    std::cout << "\n=======================================================\n";
    std::cout << " Linux Edge Gateway Running Successfully\n";
    std::cout << " Device ID:      " << config.device_id << "\n";
    std::cout << " MCU Transport:  " << config.mcu_host << ":" << config.mcu_port << "\n";
    std::cout << " CLI Monitor:    " << config.monitor_host << ":" << config.monitor_port << "\n";
    std::cout << " Modbus TCP:     " << config.modbus_host << ":" << config.modbus_port << "\n";
    std::cout << " MQTT Broker:    " << config.mqtt_host << ":" << config.mqtt_port << "\n";
    std::cout << " Watchdog:       " << config.watchdog_timeout_ms << " ms\n";
    std::cout << " Log File:       " << config.log_file << "\n";
    std::cout << "=======================================================\n\n";

    while (g_gateway_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\nStopping gateway service...\n";
    gateway.stop();
    return 0;
}

#ifndef INDUSTRIAL_MODBUS_SERVER_HPP
#define INDUSTRIAL_MODBUS_SERVER_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace industrial {

class LinuxGateway; // Forward declaration

/**
 * @brief Industrial Modbus TCP Server
 * 
 * Implements standard Modbus Application Protocol (MBAP) over TCP.
 * Exposes Holding Registers 40001 - 40008 (addresses 0x0000 - 0x0007):
 *   40001 (0x0000): Temperature x10 (°C)
 *   40002 (0x0001): Motor Current x10 (A)
 *   40003 (0x0002): Vibration x10 (mm/s)
 *   40004 (0x0003): Motor Speed (RPM)
 *   40005 (0x0004): Actuator State (0=OFF, 1=START, 2=RUN, 3=WARN, 4=ESTOP, 5=SAFE_STOP)
 *   40006 (0x0005): Fault Code
 *   40007 (0x0006): Watchdog Status (1=OK, 0=EXPIRED)
 *   40008 (0x0007): MCU Connection Status (1=CONN, 0=DISCONN)
 */
class ModbusServer {
public:
    ModbusServer(const std::string& bind_addr, int port, uint8_t unit_id, LinuxGateway& gateway);
    ~ModbusServer();

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void accept_loop();
    void handle_client(socket_t client_sock);
    std::vector<uint8_t> handle_modbus_request(const uint8_t* req, size_t len);

    std::string bind_addr_;
    int port_;
    uint8_t unit_id_;
    LinuxGateway& gateway_;

    socket_t listen_sock_{INVALID_SOCKET_FD};
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::vector<std::thread> client_threads_;
    std::mutex client_mutex_;
};

} // namespace industrial

#endif // INDUSTRIAL_MODBUS_SERVER_HPP

#ifndef INDUSTRIAL_TCP_SERVER_HPP
#define INDUSTRIAL_TCP_SERVER_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

namespace industrial {

class LinuxGateway; // Forward declaration

class CliMonitorServer {
public:
    CliMonitorServer(const std::string& host, int port, LinuxGateway& gateway);
    ~CliMonitorServer();

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void accept_loop();
    void handle_client(socket_t client_sock);
    std::string process_command(const std::string& cmd);

    std::string host_;
    int port_;
    LinuxGateway& gateway_;

    socket_t listen_sock_{INVALID_SOCKET_FD};
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::vector<std::thread> client_threads_;
    std::mutex client_mutex_;
};

} // namespace industrial

#endif // INDUSTRIAL_TCP_SERVER_HPP

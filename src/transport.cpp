#include "transport.hpp"
#include <cstring>
#include <iostream>

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/time.h>
#endif

namespace industrial {

TcpTransport::TcpTransport(const std::string& host, int port, bool is_server)
    : host_(host), port_(port), is_server_(is_server) {
    init_network_subsystem();
}

TcpTransport::~TcpTransport() {
    disconnect_endpoint();
}

bool TcpTransport::connect_endpoint() {
    if (connected_.load()) return true;

    if (is_server_) {
        listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock_ == INVALID_SOCKET_FD) {
            return false;
        }

        int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<uint16_t>(port_));
        inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);

        if (bind(listen_sock_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VAL) {
            close_socket(listen_sock_);
            listen_sock_ = INVALID_SOCKET_FD;
            return false;
        }

        if (listen(listen_sock_, 5) == SOCKET_ERROR_VAL) {
            close_socket(listen_sock_);
            listen_sock_ = INVALID_SOCKET_FD;
            return false;
        }

        return true;
    } else {
        // Client mode: connect to Gateway
        client_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_sock_ == INVALID_SOCKET_FD) {
            return false;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<uint16_t>(port_));
        inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);

        if (connect(client_sock_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VAL) {
            close_socket(client_sock_);
            client_sock_ = INVALID_SOCKET_FD;
            connected_.store(false);
            return false;
        }

        connected_.store(true);
        return true;
    }
}

bool TcpTransport::accept_client() {
    if (!is_server_ || listen_sock_ == INVALID_SOCKET_FD) {
        return false;
    }

    sockaddr_in client_addr{};
#if defined(_WIN32) || defined(_WIN64)
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif

    socket_t s = accept(listen_sock_, (struct sockaddr*)&client_addr, &addr_len);
    if (s == INVALID_SOCKET_FD) {
        return false;
    }

    if (client_sock_ != INVALID_SOCKET_FD) {
        close_socket(client_sock_);
    }

    client_sock_ = s;
    connected_.store(true);
    return true;
}

void TcpTransport::disconnect_endpoint() {
    connected_.store(false);
    if (client_sock_ != INVALID_SOCKET_FD) {
        close_socket(client_sock_);
        client_sock_ = INVALID_SOCKET_FD;
    }
    if (listen_sock_ != INVALID_SOCKET_FD) {
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCKET_FD;
    }
}

bool TcpTransport::is_connected() const {
    return connected_.load() && (client_sock_ != INVALID_SOCKET_FD);
}

int TcpTransport::send_bytes(const uint8_t* data, size_t len) {
    if (!is_connected()) return -1;
#if defined(_WIN32) || defined(_WIN64)
    int sent = send(client_sock_, (const char*)data, static_cast<int>(len), 0);
#else
    ssize_t sent = send(client_sock_, data, len, MSG_NOSIGNAL);
#endif
    if (sent <= 0) {
        connected_.store(false);
        return -1;
    }
    return static_cast<int>(sent);
}

int TcpTransport::receive_bytes(uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (!is_connected()) return -1;

    if (timeout_ms >= 0) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_sock_, &read_fds);

        struct timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int sel = select(static_cast<int>(client_sock_) + 1, &read_fds, nullptr, nullptr, &tv);
        if (sel <= 0) {
            return sel; // 0 = timeout, <0 = error
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    int received = recv(client_sock_, (char*)buffer, static_cast<int>(max_len), 0);
#else
    ssize_t received = recv(client_sock_, buffer, max_len, 0);
#endif

    if (received <= 0) {
        connected_.store(false);
        return -1;
    }
    return static_cast<int>(received);
}

} // namespace industrial

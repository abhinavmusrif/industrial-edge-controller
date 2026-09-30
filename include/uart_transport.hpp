#ifndef INDUSTRIAL_UART_TRANSPORT_HPP
#define INDUSTRIAL_UART_TRANSPORT_HPP

#include "transport.hpp"
#include <string>
#include <atomic>

namespace industrial {

/**
 * @brief Real Serial UART / RS-485 Hardware Transport
 * 
 * Implements ITransport over standard Linux serial TTY interfaces
 * (e.g. /dev/ttyUSB0, /dev/ttyS0, /dev/pts/X) using POSIX termios,
 * and Windows COM ports (e.g. COM1, \\.\COM10).
 */
class UartTransport : public ITransport {
public:
    UartTransport(const std::string& port_name, int baud_rate = 115200);
    ~UartTransport() override;

    bool connect_endpoint() override;
    void disconnect_endpoint() override;
    bool is_connected() const override;

    int send_bytes(const uint8_t* data, size_t len) override;
    int receive_bytes(uint8_t* buffer, size_t max_len, int timeout_ms = -1) override;

    std::string get_transport_name() const override {
        return "UartTransport [" + port_name_ + " @ " + std::to_string(baud_rate_) + " baud]";
    }

private:
    std::string port_name_;
    int baud_rate_;
    std::atomic<bool> connected_{false};

#if defined(_WIN32) || defined(_WIN64)
    void* handle_{nullptr}; // HANDLE
#else
    int fd_{-1};
#endif
};

} // namespace industrial

#endif // INDUSTRIAL_UART_TRANSPORT_HPP

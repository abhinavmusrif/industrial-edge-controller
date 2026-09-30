#ifndef INDUSTRIAL_TRANSPORT_HPP
#define INDUSTRIAL_TRANSPORT_HPP

#include "common.hpp"
#include <string>
#include <cstdint>
#include <vector>
#include <atomic>

namespace industrial {

/**
 * @brief Abstract Transport Interface
 * 
 * DESIGN RATIONALE:
 * In a real industrial embedded hardware deployment, the physical link between
 * the microcontroller (MCU) and Linux edge gateway is typically a serial UART,
 * RS-485 differential bus, or CAN-bus (Controller Area Network).
 * 
 * For this software digital twin, TcpTransport implements ITransport over a
 * localhost TCP socket. Higher-level framing, CRC-32 validation, and sensor
 * processing logic remain completely decoupled from the transport mechanism.
 * 
 * In hardware deployment, this transport layer can be replaced by UART/RS-485/CAN
 * (e.g. UartTransport, CanTransport) without changing the higher-level sensor/control architecture.
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool connect_endpoint() = 0;
    virtual void disconnect_endpoint() = 0;
    virtual bool is_connected() const = 0;
    
    virtual int send_bytes(const uint8_t* data, size_t len) = 0;
    virtual int receive_bytes(uint8_t* buffer, size_t max_len, int timeout_ms = -1) = 0;
    
    virtual std::string get_transport_name() const = 0;
};

class TcpTransport : public ITransport {
public:
    TcpTransport(const std::string& host, int port, bool is_server = false);
    ~TcpTransport() override;

    bool connect_endpoint() override;
    void disconnect_endpoint() override;
    bool is_connected() const override;

    int send_bytes(const uint8_t* data, size_t len) override;
    int receive_bytes(uint8_t* buffer, size_t max_len, int timeout_ms = -1) override;

    std::string get_transport_name() const override {
        return "TcpTransport [Localhost Virtual Serial Link]";
    }

    // Server-mode accept helper
    bool accept_client();
    socket_t get_socket() const { return client_sock_; }

private:
    std::string host_;
    int port_;
    bool is_server_{false};
    socket_t listen_sock_{INVALID_SOCKET_FD};
    socket_t client_sock_{INVALID_SOCKET_FD};
    std::atomic<bool> connected_{false};
};

} // namespace industrial

#endif // INDUSTRIAL_TRANSPORT_HPP

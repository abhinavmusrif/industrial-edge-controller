#include "uart_transport.hpp"
#include "logger.hpp"
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <termios.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
  #include <errno.h>
#endif

namespace industrial {

UartTransport::UartTransport(const std::string& port_name, int baud_rate)
    : port_name_(port_name), baud_rate_(baud_rate) {
}

UartTransport::~UartTransport() {
    disconnect_endpoint();
}

bool UartTransport::connect_endpoint() {
#if defined(_WIN32) || defined(_WIN64)
    std::string full_path = port_name_;
    if (full_path.rfind("\\\\.\\", 0) != 0 && full_path.length() >= 4) {
        full_path = "\\\\.\\" + full_path;
    }

    HANDLE h = CreateFileA(full_path.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL,
                           OPEN_EXISTING,
                           0,
                           NULL);
    if (h == INVALID_HANDLE_VALUE) {
        LOG_WARN("[UART] Failed to open serial port " + full_path + " (Physical hardware absent, expected in simulation mode)");
        return false;
    }

    DCB dcbSerialParams;
    memset(&dcbSerialParams, 0, sizeof(dcbSerialParams));
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(h, &dcbSerialParams)) {
        CloseHandle(h);
        return false;
    }

    dcbSerialParams.BaudRate = baud_rate_;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

    if (!SetCommState(h, &dcbSerialParams)) {
        CloseHandle(h);
        return false;
    }

    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    SetCommTimeouts(h, &timeouts);

    handle_ = static_cast<void*>(h);
    connected_ = true;
    LOG_INFO("[UART] Connected to serial port: " + port_name_ + " @ " + std::to_string(baud_rate_) + " 8N1");
    return true;

#else
    fd_ = open(port_name_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        LOG_WARN("[UART] Failed to open serial port " + port_name_ + " (Physical hardware absent, expected in simulation mode): " + strerror(errno));
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd_, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    speed_t speed = B115200;
    switch (baud_rate_) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
#ifdef B230400
        case 230400: speed = B230400; break;
#endif
        default:     speed = B115200; break;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8 data bits
    tty.c_cflag &= ~PARENB;                      // No parity
    tty.c_cflag &= ~CSTOPB;                      // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                     // No hardware RTS/CTS flow control
    tty.c_cflag |= (CLOCAL | CREAD);             // Ignore modem control, enable receiver

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);      // Disable software flow control
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Raw input

    tty.c_oflag &= ~OPOST;                       // Raw output
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN); // Non-canonical raw mode

    tty.c_cc[VMIN] = 0;                          // Non-blocking read (poll handles timeout)
    tty.c_cc[VTIME] = 1;                         // 100ms timeout

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    connected_ = true;
    LOG_INFO("[UART] Connected to serial port: " + port_name_ + " @ " + std::to_string(baud_rate_) + " 8N1");
    return true;
#endif
}

void UartTransport::disconnect_endpoint() {
    connected_ = false;
#if defined(_WIN32) || defined(_WIN64)
    if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
#else
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

bool UartTransport::is_connected() const {
    return connected_;
}

int UartTransport::send_bytes(const uint8_t* data, size_t len) {
    if (!connected_) return -1;

#if defined(_WIN32) || defined(_WIN64)
    DWORD bytes_written = 0;
    if (!WriteFile(static_cast<HANDLE>(handle_), data, static_cast<DWORD>(len), &bytes_written, NULL)) {
        return -1;
    }
    return static_cast<int>(bytes_written);
#else
    ssize_t written = ::write(fd_, data, len);
    if (written < 0) return -1;
    return static_cast<int>(written);
#endif
}

int UartTransport::receive_bytes(uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (!connected_) return -1;

#if defined(_WIN32) || defined(_WIN64)
    (void)timeout_ms;
    DWORD bytes_read = 0;
    if (!ReadFile(static_cast<HANDLE>(handle_), buffer, static_cast<DWORD>(max_len), &bytes_read, NULL)) {
        return -1;
    }
    return static_cast<int>(bytes_read);
#else
    if (timeout_ms >= 0) {
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int poll_ret = poll(&pfd, 1, timeout_ms);
        if (poll_ret <= 0) {
            return poll_ret; // 0 = timeout, <0 = error
        }
    }

    ssize_t n = ::read(fd_, buffer, max_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    return static_cast<int>(n);
#endif
}

} // namespace industrial

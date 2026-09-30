#ifndef INDUSTRIAL_COMMON_HPP
#define INDUSTRIAL_COMMON_HPP

#include <cstdint>
#include <string>
#include <chrono>
#include <memory>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  #ifdef ERROR
    #undef ERROR
  #endif
  using socket_t = SOCKET;
  constexpr socket_t INVALID_SOCKET_FD = INVALID_SOCKET;
  constexpr int SOCKET_ERROR_VAL = SOCKET_ERROR;
  inline void close_socket(socket_t s) { if (s != INVALID_SOCKET) { closesocket(s); } }
  inline void init_network_subsystem() {
      WSADATA wsaData;
      WSAStartup(MAKEWORD(2, 2), &wsaData);
  }
  inline void cleanup_network_subsystem() {
      WSACleanup();
  }
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
  #include <netdb.h>
  using socket_t = int;
  constexpr socket_t INVALID_SOCKET_FD = -1;
  constexpr int SOCKET_ERROR_VAL = -1;
  inline void close_socket(socket_t s) { if (s >= 0) { ::close(s); } }
  inline void init_network_subsystem() {}
  inline void cleanup_network_subsystem() {}
#endif

namespace industrial {

enum class ActuatorState : uint8_t {
    OFF = 0,
    STARTING = 1,
    RUNNING = 2,
    WARNING = 3,
    EMERGENCY_STOP = 4,
    SAFE_STOP = 5
};

enum class FaultCode : uint8_t {
    NONE = 0,
    HIGH_TEMPERATURE = 1,
    HIGH_CURRENT = 2,
    HIGH_VIBRATION = 3,
    SENSOR_STALE = 4,
    WATCHDOG_TIMEOUT = 5,
    CRC_ERROR = 6,
    MCU_DISCONNECTED = 7
};

enum class SensorId : uint8_t {
    TEMPERATURE = 1,
    CURRENT = 2,
    VIBRATION = 3,
    RPM = 4
};

inline const char* actuator_state_to_string(ActuatorState state) {
    switch (state) {
        case ActuatorState::OFF:            return "OFF";
        case ActuatorState::STARTING:       return "STARTING";
        case ActuatorState::RUNNING:        return "RUNNING";
        case ActuatorState::WARNING:        return "WARNING";
        case ActuatorState::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case ActuatorState::SAFE_STOP:      return "SAFE_STOP";
        default:                            return "UNKNOWN";
    }
}

inline const char* fault_code_to_string(FaultCode code) {
    switch (code) {
        case FaultCode::NONE:               return "NONE";
        case FaultCode::HIGH_TEMPERATURE:   return "HIGH_TEMPERATURE";
        case FaultCode::HIGH_CURRENT:       return "HIGH_CURRENT";
        case FaultCode::HIGH_VIBRATION:     return "HIGH_VIBRATION";
        case FaultCode::SENSOR_STALE:       return "SENSOR_STALE";
        case FaultCode::WATCHDOG_TIMEOUT:   return "WATCHDOG_TIMEOUT";
        case FaultCode::CRC_ERROR:          return "CRC_ERROR";
        case FaultCode::MCU_DISCONNECTED:   return "MCU_DISCONNECTED";
        default:                            return "UNKNOWN_FAULT";
    }
}

inline const char* sensor_id_to_string(SensorId id) {
    switch (id) {
        case SensorId::TEMPERATURE: return "TEMPERATURE";
        case SensorId::CURRENT:     return "CURRENT";
        case SensorId::VIBRATION:   return "VIBRATION";
        case SensorId::RPM:         return "RPM";
        default:                    return "UNKNOWN_SENSOR";
    }
}

inline uint64_t get_current_time_ms() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
    );
}

inline uint64_t get_steady_time_ms() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}

} // namespace industrial

#endif // INDUSTRIAL_COMMON_HPP

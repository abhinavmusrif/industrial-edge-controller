#ifndef INDUSTRIAL_TELEMETRY_HPP
#define INDUSTRIAL_TELEMETRY_HPP

#include "common.hpp"
#include "sensor.hpp"
#include "fault_manager.hpp"
#include <string>
#include <map>
#include <vector>

namespace industrial {

struct SystemTelemetry {
    std::string device_id{"device01"};
    std::string iso_timestamp;
    uint64_t epoch_ms{0};
    
    float temperature{0.0f};
    float current{0.0f};
    float vibration{0.0f};
    float rpm{0.0f};
    
    std::string controller_state{"OFF"};
    bool mcu_connected{false};
    bool watchdog_ok{false};
    uint64_t watchdog_elapsed_ms{0};
    
    std::vector<std::string> active_faults;
    std::string last_trip_reason;

    std::string to_json() const;
};

} // namespace industrial

#endif // INDUSTRIAL_TELEMETRY_HPP

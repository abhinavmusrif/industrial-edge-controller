#ifndef INDUSTRIAL_CONTROLLER_HPP
#define INDUSTRIAL_CONTROLLER_HPP

#include "common.hpp"
#include "sensor.hpp"
#include <map>
#include <mutex>
#include <string>

namespace industrial {

class ActuatorController {
public:
    ActuatorController();
    ~ActuatorController() = default;

    // Evaluates state transition rules given current sensor readings and health flags
    void evaluate(const std::map<SensorId, SensorReading>& readings,
                  const std::map<SensorId, bool>& staleness,
                  bool watchdog_ok,
                  bool mcu_connected);

    ActuatorState get_state() const;
    std::string get_state_string() const;
    std::string get_last_trip_reason() const;

    void command_start();
    void command_stop();
    void command_reset();
    void command_emergency_stop(const std::string& reason);

    void set_hysteresis_ms(uint32_t ms) { hysteresis_ms_ = ms; }
    uint32_t get_hysteresis_ms() const { return hysteresis_ms_; }

private:
    mutable std::mutex mutex_;
    ActuatorState state_{ActuatorState::OFF};
    uint64_t state_entry_time_ms_{0};
    uint64_t normal_condition_start_ms_{0};
    uint32_t hysteresis_ms_{2000};
    std::string last_trip_reason_{"None"};
};

} // namespace industrial

#endif // INDUSTRIAL_CONTROLLER_HPP

#include "controller.hpp"
#include "logger.hpp"

namespace industrial {

ActuatorController::ActuatorController()
    : state_(ActuatorState::OFF),
      state_entry_time_ms_(get_steady_time_ms()) {}

void ActuatorController::evaluate(const std::map<SensorId, SensorReading>& readings,
                                  const std::map<SensorId, bool>& staleness,
                                  bool watchdog_ok,
                                  bool mcu_connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = get_steady_time_ms();

    // 1. Critical Stop Conditions (Top Priority)
    bool has_critical_sensor = false;
    std::string crit_reason;

    for (const auto& [id, reading] : readings) {
        if (reading.is_critical) {
            has_critical_sensor = true;
            crit_reason = std::string(sensor_id_to_string(id)) + " exceeded CRITICAL threshold";
            break;
        }
    }

    if (has_critical_sensor) {
        if (state_ != ActuatorState::EMERGENCY_STOP) {
            state_ = ActuatorState::EMERGENCY_STOP;
            state_entry_time_ms_ = now;
            last_trip_reason_ = crit_reason;
            LOG_CRITICAL("Actuator TRIP -> EMERGENCY_STOP: " + crit_reason);
        }
        return;
    }

    // 2. Safe Stop Conditions (Stale sensors, watchdog timeout, disconnect)
    bool has_stale_sensor = false;
    for (const auto& [id, is_stale] : staleness) {
        if (is_stale) {
            has_stale_sensor = true;
            break;
        }
    }

    if (!watchdog_ok || !mcu_connected || has_stale_sensor) {
        if (state_ == ActuatorState::RUNNING || state_ == ActuatorState::WARNING || state_ == ActuatorState::STARTING) {
            state_ = ActuatorState::SAFE_STOP;
            state_entry_time_ms_ = now;
            if (!watchdog_ok) last_trip_reason_ = "Watchdog timeout";
            else if (!mcu_connected) last_trip_reason_ = "MCU disconnected";
            else last_trip_reason_ = "Sensor telemetry stale";
            LOG_ERROR("Actuator SAFE_STOP: " + last_trip_reason_);
        }
        return;
    }

    // 3. Normal / Warning evaluations when operating
    if (state_ == ActuatorState::EMERGENCY_STOP || state_ == ActuatorState::SAFE_STOP || state_ == ActuatorState::OFF) {
        // Locked in stopped state until commanded to start
        return;
    }

    if (state_ == ActuatorState::STARTING) {
        if (now - state_entry_time_ms_ >= 500) {
            state_ = ActuatorState::RUNNING;
            state_entry_time_ms_ = now;
            LOG_INFO("Actuator spin-up completed -> RUNNING");
        }
        return;
    }

    bool has_warning = false;
    for (const auto& [id, reading] : readings) {
        if (reading.is_warning) {
            has_warning = true;
            break;
        }
    }

    if (has_warning) {
        normal_condition_start_ms_ = 0;
        if (state_ != ActuatorState::WARNING) {
            state_ = ActuatorState::WARNING;
            state_entry_time_ms_ = now;
            LOG_WARN("Actuator state -> WARNING: Elevated sensor readings detected");
        }
    } else {
        // All sensors in nominal range. Check hysteresis before returning to RUNNING
        if (state_ == ActuatorState::WARNING) {
            if (normal_condition_start_ms_ == 0) {
                normal_condition_start_ms_ = now;
            } else if (now - normal_condition_start_ms_ >= hysteresis_ms_) {
                state_ = ActuatorState::RUNNING;
                state_entry_time_ms_ = now;
                normal_condition_start_ms_ = 0;
                LOG_INFO("Actuator recovered -> RUNNING (Hysteresis cleared)");
            }
        }
    }
}

ActuatorState ActuatorController::get_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::string ActuatorController::get_state_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return actuator_state_to_string(state_);
}

std::string ActuatorController::get_last_trip_reason() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_trip_reason_;
}

void ActuatorController::command_start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == ActuatorState::OFF || state_ == ActuatorState::SAFE_STOP) {
        state_ = ActuatorState::STARTING;
        state_entry_time_ms_ = get_steady_time_ms();
        normal_condition_start_ms_ = 0;
        LOG_INFO("Actuator command: START -> STARTING");
    }
}

void ActuatorController::command_stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ActuatorState::OFF;
    state_entry_time_ms_ = get_steady_time_ms();
    LOG_INFO("Actuator command: STOP -> OFF");
}

void ActuatorController::command_reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == ActuatorState::EMERGENCY_STOP || state_ == ActuatorState::SAFE_STOP) {
        state_ = ActuatorState::OFF;
        last_trip_reason_ = "Reset by operator";
        LOG_INFO("Actuator command: RESET -> OFF (Ready to start)");
    }
}

void ActuatorController::command_emergency_stop(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ActuatorState::EMERGENCY_STOP;
    state_entry_time_ms_ = get_steady_time_ms();
    last_trip_reason_ = reason;
    LOG_CRITICAL("Manual / Remote EMERGENCY_STOP invoked: " + reason);
}

} // namespace industrial

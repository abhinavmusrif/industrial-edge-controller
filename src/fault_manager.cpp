#include "fault_manager.hpp"
#include "logger.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace industrial {

void FaultManager::record_fault(FaultCode code, SensorId sensor, float value, float threshold,
                                const std::string& action_taken, const std::string& description) {
    FaultRecord rec;
    rec.code = code;
    rec.sensor = sensor;
    rec.value = value;
    rec.threshold = threshold;
    rec.timestamp = get_current_time_ms();
    rec.action_taken = action_taken;

    if (description.empty()) {
        std::ostringstream oss;
        oss << "CRITICAL: " << sensor_id_to_string(sensor)
            << " = " << std::fixed << std::setprecision(1) << value
            << " (Threshold = " << threshold << "), Action = " << action_taken;
        rec.description = oss.str();
    } else {
        rec.description = description;
    }

    std::vector<FaultListener> listeners_copy;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if this fault code is already active; if so update it
        auto it = std::find_if(active_faults_.begin(), active_faults_.end(),
            [code](const FaultRecord& r) { return r.code == code; });
        if (it != active_faults_.end()) {
            *it = rec;
        } else {
            active_faults_.push_back(rec);
        }

        history_.push_back(rec);
        if (history_.size() > MAX_HISTORY) {
            history_.pop_front();
        }

        listeners_copy = listeners_;
    }

    LOG_CRITICAL("FAULT: " + rec.description);

    for (const auto& listener : listeners_copy) {
        if (listener) {
            listener(rec);
        }
    }
}

void FaultManager::clear_fault(FaultCode code) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_faults_.erase(
        std::remove_if(active_faults_.begin(), active_faults_.end(),
                       [code](const FaultRecord& r) { return r.code == code; }),
        active_faults_.end()
    );
}

void FaultManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    active_faults_.clear();
}

bool FaultManager::has_active_fault() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !active_faults_.empty();
}

FaultCode FaultManager::get_highest_severity_fault() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_faults_.empty()) {
        return FaultCode::NONE;
    }
    // Return first active fault (or highest severity)
    return active_faults_.front().code;
}

std::vector<FaultRecord> FaultManager::get_active_faults() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_faults_;
}

std::vector<FaultRecord> FaultManager::get_history(size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = std::min(limit, history_.size());
    return std::vector<FaultRecord>(history_.end() - count, history_.end());
}

void FaultManager::register_listener(FaultListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.push_back(listener);
}

} // namespace industrial

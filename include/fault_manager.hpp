#ifndef INDUSTRIAL_FAULT_MANAGER_HPP
#define INDUSTRIAL_FAULT_MANAGER_HPP

#include "common.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <deque>
#include <functional>

namespace industrial {

struct FaultRecord {
    FaultCode code{FaultCode::NONE};
    SensorId sensor{SensorId::TEMPERATURE};
    float value{0.0f};
    float threshold{0.0f};
    uint64_t timestamp{0};
    std::string action_taken;
    std::string description;
};

using FaultListener = std::function<void(const FaultRecord&)>;

class FaultManager {
public:
    FaultManager() = default;
    ~FaultManager() = default;

    void record_fault(FaultCode code, SensorId sensor, float value, float threshold,
                      const std::string& action_taken, const std::string& description = "");
    
    void clear_fault(FaultCode code);
    void clear_all();

    bool has_active_fault() const;
    FaultCode get_highest_severity_fault() const;
    std::vector<FaultRecord> get_active_faults() const;
    std::vector<FaultRecord> get_history(size_t limit = 20) const;

    void register_listener(FaultListener listener);

private:
    mutable std::mutex mutex_;
    std::vector<FaultRecord> active_faults_;
    std::deque<FaultRecord> history_;
    static constexpr size_t MAX_HISTORY = 100;
    std::vector<FaultListener> listeners_;
};

} // namespace industrial

#endif // INDUSTRIAL_FAULT_MANAGER_HPP

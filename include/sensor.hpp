#ifndef INDUSTRIAL_SENSOR_HPP
#define INDUSTRIAL_SENSOR_HPP

#include "common.hpp"
#include <string>
#include <cstdint>
#include <deque>
#include <random>
#include <atomic>
#include <mutex>

namespace industrial {

struct SensorConfig {
    SensorId id;
    std::string name;
    std::string unit;
    float min_val;
    float max_val;
    float baseline;
    float normal_min;
    float normal_max;
    float warning_threshold;
    float critical_threshold;
    uint32_t period_ms;
};

struct SensorReading {
    uint64_t timestamp{0};
    SensorId id{SensorId::TEMPERATURE};
    float raw_value{0.0f};
    float filtered_value{0.0f};
    uint32_t sequence{0};
    bool is_warning{false};
    bool is_critical{false};
};

class VirtualSensor {
public:
    explicit VirtualSensor(const SensorConfig& config);
    ~VirtualSensor() = default;

    SensorReading sample();
    
    // Fault injection overrides
    void inject_override(float forced_value, uint32_t duration_ms = 0);
    void clear_override();
    bool is_override_active() const;

    const SensorConfig& get_config() const { return config_; }
    uint32_t get_period_ms() const { return config_.period_ms; }

private:
    float generate_smooth_variation();
    float apply_filter(float new_val);

    SensorConfig config_;
    uint32_t sequence_{0};
    double simulation_time_{0.0};
    float current_filtered_{0.0f};
    
    // Deterministic pseudo-random generator
    std::mt19937 rng_;
    std::normal_distribution<float> noise_dist_{0.0f, 0.05f};
    
    // Moving average filter buffer
    std::deque<float> filter_window_;
    static constexpr size_t FILTER_WINDOW_SIZE = 5;

    // Thread-safe override state
    mutable std::mutex mutex_;
    bool has_override_{false};
    float override_value_{0.0f};
    uint64_t override_expiry_ms_{0};
};

} // namespace industrial

#endif // INDUSTRIAL_SENSOR_HPP

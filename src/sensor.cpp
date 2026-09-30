#include "sensor.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace industrial {

VirtualSensor::VirtualSensor(const SensorConfig& config)
    : config_(config),
      current_filtered_(config.baseline),
      rng_(1337 + static_cast<uint32_t>(config.id)) {
    // Seed filter window with baseline
    for (size_t i = 0; i < FILTER_WINDOW_SIZE; ++i) {
        filter_window_.push_back(config_.baseline);
    }
}

float VirtualSensor::generate_smooth_variation() {
    simulation_time_ += 0.05;

    // Realistic physical dynamics: combination of slow thermal/mechanical drift
    // and slight periodic fluctuations
    double omega1 = 0.15;
    double omega2 = 0.42;
    float range_span = config_.normal_max - config_.normal_min;
    float amplitude = range_span * 0.15f;

    float wave = static_cast<float>(
        std::sin(omega1 * simulation_time_) * 0.7 +
        std::cos(omega2 * simulation_time_) * 0.3
    );

    float noise = noise_dist_(rng_) * (range_span * 0.03f);
    float value = config_.baseline + (wave * amplitude) + noise;

    // Clamp within sensor physical limits
    return std::clamp(value, config_.min_val, config_.max_val);
}

float VirtualSensor::apply_filter(float new_val) {
    if (filter_window_.size() >= FILTER_WINDOW_SIZE) {
        filter_window_.pop_front();
    }
    filter_window_.push_back(new_val);

    float sum = std::accumulate(filter_window_.begin(), filter_window_.end(), 0.0f);
    return sum / static_cast<float>(filter_window_.size());
}

SensorReading VirtualSensor::sample() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SensorReading reading;
    reading.timestamp = get_current_time_ms();
    reading.id = config_.id;
    reading.sequence = ++sequence_;

    float raw = 0.0f;
    uint64_t now = get_current_time_ms();

    if (has_override_) {
        if (override_expiry_ms_ > 0 && now >= override_expiry_ms_) {
            has_override_ = false;
            raw = generate_smooth_variation();
        } else {
            raw = override_value_;
        }
    } else {
        raw = generate_smooth_variation();
    }

    reading.raw_value = raw;
    reading.filtered_value = apply_filter(raw);

    // Evaluate thresholds against filtered value (or raw if sudden critical)
    float eval_val = reading.filtered_value;
    if (eval_val >= config_.critical_threshold) {
        reading.is_critical = true;
        reading.is_warning = true;
    } else if (eval_val >= config_.warning_threshold) {
        reading.is_warning = true;
        reading.is_critical = false;
    } else {
        reading.is_warning = false;
        reading.is_critical = false;
    }

    return reading;
}

void VirtualSensor::inject_override(float forced_value, uint32_t duration_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    has_override_ = true;
    override_value_ = forced_value;
    override_expiry_ms_ = (duration_ms > 0) ? (get_current_time_ms() + duration_ms) : 0;
}

void VirtualSensor::clear_override() {
    std::lock_guard<std::mutex> lock(mutex_);
    has_override_ = false;
    override_value_ = 0.0f;
    override_expiry_ms_ = 0;
}

bool VirtualSensor::is_override_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return has_override_;
}

} // namespace industrial

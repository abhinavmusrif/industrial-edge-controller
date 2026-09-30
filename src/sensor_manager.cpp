#include "sensor_manager.hpp"
#include <chrono>

namespace industrial {

SensorManager::SensorManager() {
    initialize_default_sensors();
}

SensorManager::~SensorManager() {
    stop();
}

void SensorManager::initialize_default_sensors() {
    // Temperature: 20-100 C, normal 35-70, warn 70-85, crit >85, period 500ms
    add_sensor({
        SensorId::TEMPERATURE, "Temperature", "C",
        20.0f, 100.0f, 48.5f, 35.0f, 70.0f, 70.0f, 85.0f, 500
    });

    // Motor Current: 0-20 A, normal 2-10, warn 10-15, crit >15, period 200ms
    add_sensor({
        SensorId::CURRENT, "Motor Current", "A",
        0.0f, 20.0f, 6.2f, 2.0f, 10.0f, 10.0f, 15.0f, 200
    });

    // Vibration: 0-15 mm/s, normal 0-4, warn 4-7, crit >7, period 100ms
    add_sensor({
        SensorId::VIBRATION, "Vibration", "mm/s",
        0.0f, 15.0f, 1.8f, 0.0f, 4.0f, 4.0f, 7.0f, 100
    });

    // Motor RPM: 0-3600 RPM, normal 500-3000, warn 2800, crit 3200, period 100ms
    add_sensor({
        SensorId::RPM, "Motor RPM", "RPM",
        0.0f, 3600.0f, 1800.0f, 500.0f, 3000.0f, 2800.0f, 3200.0f, 100
    });
}

void SensorManager::add_sensor(const SensorConfig& config) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    sensors_[config.id] = std::make_unique<VirtualSensor>(config);
}

void SensorManager::set_sample_callback(SensorCallback callback) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    callback_ = callback;
}

void SensorManager::start() {
    if (running_.load()) return;
    running_.store(true);

    std::lock_guard<std::mutex> lock(data_mutex_);
    task_threads_.clear();
    for (const auto& [id, sensor] : sensors_) {
        uint32_t period = sensor->get_period_ms();
        task_threads_.emplace_back(&SensorManager::run_periodic_task, this, id, period);
    }
}

void SensorManager::stop() {
    if (!running_.load()) return;
    running_.store(false);

    for (auto& th : task_threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
    task_threads_.clear();
}

void SensorManager::run_periodic_task(SensorId id, uint32_t period_ms) {
    using namespace std::chrono;
    auto period = milliseconds(period_ms);
    auto next_deadline = steady_clock::now();

    while (running_.load()) {
        SensorReading reading;
        SensorCallback cb;

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            auto it = sensors_.find(id);
            if (it != sensors_.end()) {
                reading = it->second->sample();
                latest_readings_[id] = reading;
            }
            cb = callback_;
        }

        if (cb) {
            cb(reading);
        }

        // Deadline-based periodic task scheduling:
        // Ensures strict periodic execution free of cumulative drift.
        next_deadline += period;
        auto now = steady_clock::now();
        if (next_deadline < now) {
            // Missed deadline recovery
            next_deadline = now + period;
        }
        std::this_thread::sleep_until(next_deadline);
    }
}

bool SensorManager::inject_sensor_fault(SensorId id, float override_val, uint32_t duration_ms) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = sensors_.find(id);
    if (it != sensors_.end()) {
        it->second->inject_override(override_val, duration_ms);
        return true;
    }
    return false;
}

bool SensorManager::clear_sensor_fault(SensorId id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = sensors_.find(id);
    if (it != sensors_.end()) {
        it->second->clear_override();
        return true;
    }
    return false;
}

void SensorManager::clear_all_faults() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    for (auto& [id, sensor] : sensors_) {
        sensor->clear_override();
    }
}

SensorReading SensorManager::get_latest_sample(SensorId id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = latest_readings_.find(id);
    if (it != latest_readings_.end()) {
        return it->second;
    }
    return SensorReading{};
}

} // namespace industrial

#ifndef INDUSTRIAL_SENSOR_MANAGER_HPP
#define INDUSTRIAL_SENSOR_MANAGER_HPP

#include "sensor.hpp"
#include <map>
#include <vector>
#include <thread>
#include <functional>
#include <atomic>

namespace industrial {

using SensorCallback = std::function<void(const SensorReading&)>;

class SensorManager {
public:
    SensorManager();
    ~SensorManager();

    void initialize_default_sensors();
    void add_sensor(const SensorConfig& config);
    
    // Set callback invoked whenever any sensor completes a periodic sample
    void set_sample_callback(SensorCallback callback);

    // Start/Stop periodic sampling threads
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Fault injection delegation
    bool inject_sensor_fault(SensorId id, float override_val, uint32_t duration_ms = 0);
    bool clear_sensor_fault(SensorId id);
    void clear_all_faults();

    // Direct sample querying
    SensorReading get_latest_sample(SensorId id);

private:
    void run_periodic_task(SensorId id, uint32_t period_ms);

    std::map<SensorId, std::unique_ptr<VirtualSensor>> sensors_;
    std::map<SensorId, SensorReading> latest_readings_;
    mutable std::mutex data_mutex_;

    std::vector<std::thread> task_threads_;
    std::atomic<bool> running_{false};
    SensorCallback callback_;
};

} // namespace industrial

#endif // INDUSTRIAL_SENSOR_MANAGER_HPP

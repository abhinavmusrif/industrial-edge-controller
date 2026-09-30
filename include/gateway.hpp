#ifndef INDUSTRIAL_GATEWAY_HPP
#define INDUSTRIAL_GATEWAY_HPP

#include "common.hpp"
#include "protocol.hpp"
#include "sensor.hpp"
#include "controller.hpp"
#include "watchdog.hpp"
#include "fault_manager.hpp"
#include "transport.hpp"
#include "telemetry.hpp"
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

namespace industrial {

class CliMonitorServer;
class ModbusServer;
class MqttClient;

struct GatewayConfig {
    std::string device_id{"device01"};
    std::string mcu_host{"127.0.0.1"};
    int mcu_port{9000};
    
    std::string monitor_host{"127.0.0.1"};
    int monitor_port{9100};

    std::string modbus_host{"0.0.0.0"};
    int modbus_port{1502};
    uint8_t modbus_unit_id{1};
    bool modbus_enabled{true};

    std::string mqtt_host{"127.0.0.1"};
    int mqtt_port{1883};
    std::string mqtt_topic_prefix{"industrial/device01"};
    bool mqtt_enabled{true};

    uint32_t watchdog_timeout_ms{2000};
    uint32_t stale_timeout_ms{1500};
    uint32_t control_cycle_ms{20};
    uint32_t recovery_hysteresis_ms{2000};

    std::string log_level{"INFO"};
    std::string log_file{"logs/gateway.log"};

    static GatewayConfig load_from_file(const std::string& path);
};

struct GatewayStats {
    std::atomic<uint64_t> frames_received{0};
    std::atomic<uint64_t> crc_errors{0};
    std::atomic<uint64_t> malformed_packets{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint32_t> watchdog_trips{0};
    std::atomic<uint32_t> estop_trips{0};
};

class LinuxGateway {
public:
    explicit LinuxGateway(const GatewayConfig& config);
    ~LinuxGateway();

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

    // State queries for servers and monitoring
    SystemTelemetry get_current_telemetry() const;
    std::map<SensorId, SensorReading> get_all_sensors() const;
    SensorReading get_sensor(SensorId id) const;
    ActuatorState get_actuator_state() const;
    std::string get_actuator_state_str() const;
    bool is_mcu_connected() const { return mcu_connected_.load(); }
    bool is_watchdog_ok() const;
    uint64_t get_watchdog_elapsed_ms() const;
    
    FaultManager& get_fault_manager() { return fault_mgr_; }
    ActuatorController& get_controller() { return controller_; }
    const GatewayConfig& get_config() const { return config_; }
    const GatewayStats& get_stats() const { return stats_; }

private:
    // Dedicated Worker Threads
    void mcu_communication_thread();
    void sensor_health_thread();
    void control_loop_thread();
    void telemetry_publish_thread();
    void watchdog_monitor_thread();

    void on_frame_received(const Frame& frame);
    void on_sensor_sample_decoded(const SensorPayload& payload, uint32_t sequence);

    GatewayConfig config_;
    GatewayStats stats_;
    std::atomic<bool> running_{false};
    std::atomic<bool> mcu_connected_{false};

    // Subsystems
    Watchdog watchdog_;
    FaultManager fault_mgr_;
    ActuatorController controller_;

    std::unique_ptr<TcpTransport> mcu_transport_;
    std::unique_ptr<CliMonitorServer> monitor_server_;
    std::unique_ptr<ModbusServer> modbus_server_;
    std::unique_ptr<MqttClient> mqtt_client_;

    // Sensor State Tables
    mutable std::mutex data_mutex_;
    std::map<SensorId, SensorReading> latest_readings_;
    std::map<SensorId, uint64_t> last_sensor_update_ms_;
    std::map<SensorId, bool> sensor_stale_;

    // Thread handles
    std::thread mcu_comm_th_;
    std::thread health_th_;
    std::thread control_th_;
    std::thread telemetry_th_;
    std::thread watchdog_th_;
};

} // namespace industrial

#endif // INDUSTRIAL_GATEWAY_HPP

#include "gateway.hpp"
#include "tcp_server.hpp"
#include "modbus_server.hpp"
#include "mqtt_client.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace industrial {

GatewayConfig GatewayConfig::load_from_file(const std::string& path) {
    GatewayConfig cfg;
    std::ifstream file(path);
    if (!file.is_open()) {
        return cfg; // Default configuration
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Trim whitespace
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t' || key.back() == '\r')) key.pop_back();
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r')) val.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());

        if (key == "device_id") cfg.device_id = val;
        else if (key == "mcu_host") cfg.mcu_host = val;
        else if (key == "mcu_port") cfg.mcu_port = std::stoi(val);
        else if (key == "monitor_host") cfg.monitor_host = val;
        else if (key == "monitor_port") cfg.monitor_port = std::stoi(val);
        else if (key == "modbus_host") cfg.modbus_host = val;
        else if (key == "modbus_port") cfg.modbus_port = std::stoi(val);
        else if (key == "modbus_unit_id") cfg.modbus_unit_id = static_cast<uint8_t>(std::stoi(val));
        else if (key == "modbus_enabled") cfg.modbus_enabled = (val == "true" || val == "1");
        else if (key == "mqtt_host") cfg.mqtt_host = val;
        else if (key == "mqtt_port") cfg.mqtt_port = std::stoi(val);
        else if (key == "mqtt_topic_prefix") cfg.mqtt_topic_prefix = val;
        else if (key == "mqtt_enabled") cfg.mqtt_enabled = (val == "true" || val == "1");
        else if (key == "watchdog_timeout_ms") cfg.watchdog_timeout_ms = std::stoul(val);
        else if (key == "stale_timeout_ms") cfg.stale_timeout_ms = std::stoul(val);
        else if (key == "control_cycle_ms") cfg.control_cycle_ms = std::stoul(val);
        else if (key == "recovery_hysteresis_ms") cfg.recovery_hysteresis_ms = std::stoul(val);
        else if (key == "log_level") cfg.log_level = val;
        else if (key == "log_file") cfg.log_file = val;
    }

    return cfg;
}

LinuxGateway::LinuxGateway(const GatewayConfig& config)
    : config_(config),
      watchdog_(config.watchdog_timeout_ms) {
    Logger::instance().init(config_.log_file, Logger::parse_level(config_.log_level), true);
    controller_.set_hysteresis_ms(config_.recovery_hysteresis_ms);

    // Watchdog timeout callback
    watchdog_.set_timeout_callback([this](uint64_t elapsed_ms, uint64_t timeout_ms) {
        stats_.watchdog_trips++;
        fault_mgr_.record_fault(
            FaultCode::WATCHDOG_TIMEOUT,
            SensorId::TEMPERATURE,
            static_cast<float>(elapsed_ms),
            static_cast<float>(timeout_ms),
            "SAFE_STOP",
            "CRITICAL: Watchdog heartbeat missing (" + std::to_string(elapsed_ms) + " ms elapsed), Action = SAFE_STOP"
        );
    });

    // Subsystems
    mcu_transport_ = std::make_unique<TcpTransport>(config_.mcu_host, config_.mcu_port, true);
    monitor_server_ = std::make_unique<CliMonitorServer>(config_.monitor_host, config_.monitor_port, *this);
    
    if (config_.modbus_enabled) {
        modbus_server_ = std::make_unique<ModbusServer>(config_.modbus_host, config_.modbus_port, config_.modbus_unit_id, *this);
    }

    if (config_.mqtt_enabled) {
        mqtt_client_ = std::make_unique<MqttClient>(config_.mqtt_host, config_.mqtt_port, "edge-gateway-" + config_.device_id, config_.mqtt_topic_prefix);
    }
}

LinuxGateway::~LinuxGateway() {
    stop();
}

bool LinuxGateway::start() {
    if (running_.load()) return true;
    running_.store(true);

    LOG_INFO("==================================================");
    LOG_INFO("Starting Linux Edge Gateway [" + config_.device_id + "]");
    LOG_INFO("==================================================");

    // 1. Start MCU Transport Server (port 9000)
    if (!mcu_transport_->connect_endpoint()) {
        LOG_CRITICAL("Failed to bind MCU transport socket on " + config_.mcu_host + ":" + std::to_string(config_.mcu_port));
        return false;
    }
    LOG_INFO("MCU Transport listening on " + config_.mcu_host + ":" + std::to_string(config_.mcu_port));

    // 2. Start CLI Monitor Server (port 9100)
    if (monitor_server_) {
        monitor_server_->start();
    }

    // 3. Start Modbus TCP Server (port 1502)
    if (modbus_server_) {
        modbus_server_->start();
    }

    // 4. Try initial MQTT connection (non-fatal if broker is not running)
    if (mqtt_client_) {
        if (!mqtt_client_->connect_broker()) {
            LOG_WARN("MQTT broker not available at startup. Telemetry will retry in background.");
        }
    }

    // Set initial controller state to starting/running
    controller_.command_start();

    // 5. Launch Gateway worker threads
    mcu_comm_th_ = std::thread(&LinuxGateway::mcu_communication_thread, this);
    health_th_   = std::thread(&LinuxGateway::sensor_health_thread, this);
    control_th_  = std::thread(&LinuxGateway::control_loop_thread, this);
    telemetry_th_= std::thread(&LinuxGateway::telemetry_publish_thread, this);
    watchdog_th_ = std::thread(&LinuxGateway::watchdog_monitor_thread, this);

    LOG_INFO("Linux Edge Gateway initialized with 6 core worker threads");
    return true;
}

void LinuxGateway::stop() {
    if (!running_.load()) return;
    running_.store(false);

    LOG_INFO("Shutting down Linux Edge Gateway...");

    if (mcu_transport_) mcu_transport_->disconnect_endpoint();
    if (monitor_server_) monitor_server_->stop();
    if (modbus_server_) modbus_server_->stop();
    if (mqtt_client_) mqtt_client_->disconnect_broker();

    if (mcu_comm_th_.joinable()) mcu_comm_th_.join();
    if (health_th_.joinable()) health_th_.join();
    if (control_th_.joinable()) control_th_.join();
    if (telemetry_th_.joinable()) telemetry_th_.join();
    if (watchdog_th_.joinable()) watchdog_th_.join();

    LOG_INFO("Linux Edge Gateway stopped cleanly.");
}

void LinuxGateway::mcu_communication_thread() {
    std::vector<uint8_t> rx_buffer;
    uint8_t temp_buf[512];

    while (running_.load()) {
        if (!mcu_transport_->is_connected()) {
            mcu_connected_.store(false);
            if (mcu_transport_->accept_client()) {
                mcu_connected_.store(true);
                watchdog_.kick();
                LOG_INFO("MCU connected to simulated serial/UART TCP link");
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        int bytes = mcu_transport_->receive_bytes(temp_buf, sizeof(temp_buf), 200);
        if (bytes > 0) {
            stats_.bytes_received += bytes;
            rx_buffer.insert(rx_buffer.end(), temp_buf, temp_buf + bytes);

            while (true) {
                Frame frame;
                DecodeResult res = Protocol::deserialize_frame(rx_buffer, frame);
                if (res == DecodeResult::SUCCESS) {
                    stats_.frames_received++;
                    watchdog_.kick();
                    on_frame_received(frame);
                } else if (res == DecodeResult::CORRUPTED_CRC) {
                    stats_.crc_errors++;
                    fault_mgr_.record_fault(
                        FaultCode::CRC_ERROR,
                        SensorId::TEMPERATURE,
                        0.0f, 0.0f,
                        "REJECT_FRAME",
                        "CRITICAL: CRC-32 checksum mismatch detected, frame rejected!"
                    );
                    break;
                } else if (res == DecodeResult::NEED_MORE_DATA) {
                    break;
                } else {
                    stats_.malformed_packets++;
                    break;
                }
            }
        } else if (bytes < 0) {
            mcu_connected_.store(false);
            LOG_WARN("MCU disconnected from transport link");
            fault_mgr_.record_fault(
                FaultCode::MCU_DISCONNECTED,
                SensorId::TEMPERATURE,
                0.0f, 0.0f,
                "SAFE_STOP",
                "CRITICAL: MCU physical/virtual link disconnected!"
            );
        }
    }
}

void LinuxGateway::on_frame_received(const Frame& frame) {
    if (frame.message_type == static_cast<uint8_t>(MessageType::SENSOR_DATA)) {
        SensorPayload payload;
        if (Protocol::parse_sensor_payload(frame.payload, payload)) {
            on_sensor_sample_decoded(payload, frame.sequence);
        }
    } else if (frame.message_type == static_cast<uint8_t>(MessageType::HEARTBEAT)) {
        LOG_DEBUG("Heartbeat frame received from MCU [Seq=" + std::to_string(frame.sequence) + "]");
    }
}

void LinuxGateway::on_sensor_sample_decoded(const SensorPayload& payload, uint32_t sequence) {
    SensorReading r;
    r.timestamp = get_current_time_ms();
    r.id = static_cast<SensorId>(payload.sensor_id);
    r.raw_value = payload.raw_value;
    r.filtered_value = payload.filtered_value;
    r.sequence = sequence;
    r.is_warning = (payload.status_flags & 0x01) != 0;
    r.is_critical = (payload.status_flags & 0x02) != 0;

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        latest_readings_[r.id] = r;
        last_sensor_update_ms_[r.id] = get_steady_time_ms();
        sensor_stale_[r.id] = false;
    }

    // Check fault triggers
    if (r.is_critical) {
        FaultCode fc = FaultCode::NONE;
        float thresh = 0.0f;
        if (r.id == SensorId::TEMPERATURE) { fc = FaultCode::HIGH_TEMPERATURE; thresh = 85.0f; }
        else if (r.id == SensorId::CURRENT) { fc = FaultCode::HIGH_CURRENT; thresh = 15.0f; }
        else if (r.id == SensorId::VIBRATION) { fc = FaultCode::HIGH_VIBRATION; thresh = 7.0f; }

        if (fc != FaultCode::NONE) {
            stats_.estop_trips++;
            fault_mgr_.record_fault(fc, r.id, r.filtered_value, thresh, "EMERGENCY_STOP");
        }
    }
}

void LinuxGateway::sensor_health_thread() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        uint64_t now = get_steady_time_ms();
        std::lock_guard<std::mutex> lock(data_mutex_);

        for (auto& [id, last_time] : last_sensor_update_ms_) {
            if (now - last_time > config_.stale_timeout_ms) {
                if (!sensor_stale_[id]) {
                    sensor_stale_[id] = true;
                    fault_mgr_.record_fault(
                        FaultCode::SENSOR_STALE,
                        id,
                        static_cast<float>(now - last_time),
                        static_cast<float>(config_.stale_timeout_ms),
                        "SAFE_STOP",
                        "CRITICAL: Sensor " + std::string(sensor_id_to_string(id)) + " is STALE (" +
                        std::to_string(now - last_time) + " ms without update), Action = SAFE_STOP"
                    );
                }
            }
        }
    }
}

void LinuxGateway::control_loop_thread() {
    using namespace std::chrono;
    auto period = milliseconds(config_.control_cycle_ms);
    auto next_deadline = steady_clock::now();

    while (running_.load()) {
        std::map<SensorId, SensorReading> readings;
        std::map<SensorId, bool> staleness;

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            readings = latest_readings_;
            staleness = sensor_stale_;
        }

        controller_.evaluate(readings, staleness, watchdog_.is_healthy(), mcu_connected_.load());

        next_deadline += period;
        auto now = steady_clock::now();
        if (next_deadline < now) {
            next_deadline = now + period;
        }
        std::this_thread::sleep_until(next_deadline);
    }
}

void LinuxGateway::telemetry_publish_thread() {
    uint64_t last_mqtt_retry_ms = 0;

    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto telemetry = get_current_telemetry();
        std::string json_data = telemetry.to_json();

        // MQTT Handling
        if (mqtt_client_ && mqtt_client_->is_enabled()) {
            if (!mqtt_client_->is_connected()) {
                uint64_t now = get_steady_time_ms();
                if (now - last_mqtt_retry_ms > 5000) {
                    last_mqtt_retry_ms = now;
                    mqtt_client_->connect_broker();
                }
            }

            if (mqtt_client_->is_connected()) {
                mqtt_client_->publish("telemetry", json_data);
                mqtt_client_->publish("temperature", std::to_string(telemetry.temperature));
                mqtt_client_->publish("current", std::to_string(telemetry.current));
                mqtt_client_->publish("vibration", std::to_string(telemetry.vibration));
                mqtt_client_->publish("rpm", std::to_string(telemetry.rpm));
                mqtt_client_->publish("status", "{\"state\":\"" + telemetry.controller_state + "\"}");
                if (!telemetry.active_faults.empty()) {
                    mqtt_client_->publish("fault", "{\"fault\":\"" + telemetry.active_faults.front() + "\"}");
                }
            }
        }
    }
}

void LinuxGateway::watchdog_monitor_thread() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        watchdog_.check();
    }
}

SystemTelemetry LinuxGateway::get_current_telemetry() const {
    SystemTelemetry t;
    t.device_id = config_.device_id;
    t.epoch_ms = get_current_time_ms();

    // ISO timestamp
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);
    std::tm bt{};
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&bt, &timer);
#else
    gmtime_r(&timer, &bt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    t.iso_timestamp = oss.str();

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it_t = latest_readings_.find(SensorId::TEMPERATURE);
        if (it_t != latest_readings_.end()) t.temperature = it_t->second.filtered_value;

        auto it_c = latest_readings_.find(SensorId::CURRENT);
        if (it_c != latest_readings_.end()) t.current = it_c->second.filtered_value;

        auto it_v = latest_readings_.find(SensorId::VIBRATION);
        if (it_v != latest_readings_.end()) t.vibration = it_v->second.filtered_value;

        auto it_r = latest_readings_.find(SensorId::RPM);
        if (it_r != latest_readings_.end()) t.rpm = it_r->second.filtered_value;
    }

    t.controller_state = controller_.get_state_string();
    t.mcu_connected = mcu_connected_.load();
    t.watchdog_ok = watchdog_.is_healthy();
    t.watchdog_elapsed_ms = watchdog_.get_time_since_last_kick_ms();
    t.last_trip_reason = controller_.get_last_trip_reason();

    auto active = fault_mgr_.get_active_faults();
    for (const auto& f : active) {
        t.active_faults.push_back(f.description);
    }

    return t;
}

std::map<SensorId, SensorReading> LinuxGateway::get_all_sensors() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return latest_readings_;
}

SensorReading LinuxGateway::get_sensor(SensorId id) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = latest_readings_.find(id);
    if (it != latest_readings_.end()) {
        return it->second;
    }
    return SensorReading{};
}

ActuatorState LinuxGateway::get_actuator_state() const {
    return controller_.get_state();
}

std::string LinuxGateway::get_actuator_state_str() const {
    return controller_.get_state_string();
}

bool LinuxGateway::is_watchdog_ok() const {
    return watchdog_.is_healthy();
}

uint64_t LinuxGateway::get_watchdog_elapsed_ms() const {
    return watchdog_.get_time_since_last_kick_ms();
}

} // namespace industrial

#include "hardware_items.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace industrial {

HardwareModel& HardwareModel::instance() {
    static HardwareModel inst;
    return inst;
}

HardwareModel::HardwareModel() {
    initialize();
}

void HardwareModel::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;

    // 1. TMP117 High-Precision Digital Temperature Sensor (Texas Instruments)
    HardwareDevice tmp117;
    tmp117.name = "TMP117";
    tmp117.part_number = "TMP117AIDRVR";
    tmp117.bus_type = "I2C-1";
    tmp117.bus_address = 0x48;
    tmp117.description = "High-precision ±0.1°C temperature sensor with 16-bit ADC";
    
    tmp117.registers[0x00] = {0x00, "TEMP_RESULT",  0x1900, "Temperature Data Register (7.8125 m°C/LSB)", "°C", 0.0078125f, true};
    tmp117.registers[0x01] = {0x01, "CONFIG",       0x0220, "Configuration Register (Continuous conversion, 16-avg)", "", 1.0f, false};
    tmp117.registers[0x02] = {0x02, "THIGH_LIMIT",  0x2A80, "Temperature High Limit Alert Register (85.0°C)", "°C", 0.0078125f, false};
    tmp117.registers[0x03] = {0x03, "TLOW_LIMIT",   0x1180, "Temperature Low Limit Alert Register (35.0°C)", "°C", 0.0078125f, false};
    devices_["TMP117"] = tmp117;

    // 2. INA219 High-Side DC Current & Power Monitor (Texas Instruments)
    HardwareDevice ina219;
    ina219.name = "INA219";
    ina219.part_number = "INA219BIDR";
    ina219.bus_type = "I2C-1";
    ina219.bus_address = 0x40;
    ina219.description = "Precision bi-directional current shunt and power monitor";

    ina219.registers[0x00] = {0x00, "CONFIG",        0x399F, "Configuration (32V FSR, ±320mV Shunt, 12-bit)", "", 1.0f, false};
    ina219.registers[0x01] = {0x01, "SHUNT_VOLTAGE", 0x0294, "Shunt Voltage Drop (10 uV/LSB)", "mV", 0.01f, true};
    ina219.registers[0x02] = {0x02, "BUS_VOLTAGE",   0x1770, "Motor DC Bus Voltage (4 mV/LSB = 24.0V)", "V", 0.004f, true};
    ina219.registers[0x04] = {0x04, "CURRENT_RAW",   0x0294, "Motor Load Current (10 mA/LSB)", "A", 0.01f, true};
    devices_["INA219"] = ina219;

    // 3. MPU-6050 6-Axis MotionTracking / Vibration Accelerometer (InvenSense)
    HardwareDevice mpu6050;
    mpu6050.name = "MPU6050";
    mpu6050.part_number = "MPU-6050";
    mpu6050.bus_type = "I2C-1";
    mpu6050.bus_address = 0x68;
    mpu6050.description = "Triple-axis MEMS accelerometer with onboard digital motion processor";

    mpu6050.registers[0x3B] = {0x3B, "ACCEL_XOUT",  0x0080, "Raw X-Axis Acceleration", "g", 0.000061f, true};
    mpu6050.registers[0x3D] = {0x3D, "ACCEL_YOUT",  0x0060, "Raw Y-Axis Acceleration", "g", 0.000061f, true};
    mpu6050.registers[0x3F] = {0x3F, "ACCEL_ZOUT",  0x4000, "Raw Z-Axis Acceleration (Gravity reference)", "g", 0.000061f, true};
    mpu6050.registers[0x41] = {0x41, "VIBE_RMS",    0x00CD, "Calculated Vibration RMS (10 um/s / LSB)", "mm/s", 0.01f, true};
    mpu6050.registers[0x75] = {0x75, "WHO_AM_I",    0x0068, "Device Identification Verification Register", "", 1.0f, true};
    devices_["MPU6050"] = mpu6050;

    // 4. Optical Shaft Quadrature Encoder (Timer Counter)
    HardwareDevice encoder;
    encoder.name = "ENCODER";
    encoder.part_number = "TIM2_ENCODER";
    encoder.bus_type = "TIM-2";
    encoder.bus_address = 0x00;
    encoder.description = "Hardware 32-bit quadrature timer counter coupled to motor shaft";

    encoder.registers[0x00] = {0x00, "TIM_CNT",    0x4E20, "Current Quadrature Edge Count", "pulses", 1.0f, true};
    encoder.registers[0x04] = {0x04, "SPEED_RPM",  0x079E, "Computed Motor Rotational Speed", "RPM", 1.0f, true};
    encoder.registers[0x08] = {0x08, "TIM_ARR",    0xFFFF, "Auto-Reload Register (Max pulse modulus)", "", 1.0f, false};
    devices_["ENCODER"] = encoder;

    // 5. Virtual GPIO Port A
    gpio_bank_[0] = {0, "MOTOR_PWM_ENABLE",    "OUT", 1, "H-Bridge Gate Drive Enable Line (Active HIGH)"};
    gpio_bank_[1] = {1, "ESTOP_RELAY_TRIP",    "OUT", 0, "Emergency Stop Contactor Relay (0=Normal Closed, 1=Open/Trip)"};
    gpio_bank_[2] = {2, "WATCHDOG_WDI",        "OUT", 1, "External Watchdog Heartbeat Input Line (Toggling)"};
    gpio_bank_[3] = {3, "OPERATOR_RESET_PB",   "IN",  0, "Physical Control Cabinet Push Button (Momentary Active HIGH)"};
    gpio_bank_[4] = {4, "WARNING_BEACON_LED",  "OUT", 0, "Cabinet Yellow Warning Tower Beacon (0=OFF, 1=ON)"};

    initialized_ = true;
}

void HardwareModel::update_from_sensors(float temp, float current, float vibe, float rpm, ActuatorState act_state, bool watchdog_ok) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Update TMP117
    auto it_t = devices_.find("TMP117");
    if (it_t != devices_.end()) {
        uint16_t raw_temp = static_cast<uint16_t>(temp / 0.0078125f);
        it_t->second.registers[0x00].value = raw_temp;
    }

    // Update INA219
    auto it_c = devices_.find("INA219");
    if (it_c != devices_.end()) {
        uint16_t raw_curr = static_cast<uint16_t>(current / 0.01f);
        it_c->second.registers[0x04].value = raw_curr;
        it_c->second.registers[0x01].value = static_cast<uint16_t>((current * 0.01f) / 0.00001f); // 10mOhm shunt
    }

    // Update MPU6050
    auto it_v = devices_.find("MPU6050");
    if (it_v != devices_.end()) {
        uint16_t raw_vibe = static_cast<uint16_t>(vibe / 0.01f);
        it_v->second.registers[0x41].value = raw_vibe;
    }

    // Update Encoder
    auto it_e = devices_.find("ENCODER");
    if (it_e != devices_.end()) {
        it_e->second.registers[0x04].value = static_cast<uint16_t>(rpm);
    }

    // Update GPIO States based on actuator safety state
    if (act_state == ActuatorState::RUNNING || act_state == ActuatorState::STARTING) {
        gpio_bank_[0].state = 1; // Motor PWM active
        gpio_bank_[1].state = 0; // ESTOP Relay closed
        gpio_bank_[4].state = 0; // Beacon OFF
    } else if (act_state == ActuatorState::WARNING) {
        gpio_bank_[0].state = 1; // Motor PWM still running
        gpio_bank_[1].state = 0; // Relay closed
        gpio_bank_[4].state = 1; // Warning Beacon FLASHING/ON
    } else if (act_state == ActuatorState::EMERGENCY_STOP || act_state == ActuatorState::SAFE_STOP) {
        gpio_bank_[0].state = 0; // Motor PWM KILLED
        gpio_bank_[1].state = 1; // ESTOP Relay TRIPPED OPEN
        gpio_bank_[4].state = 1; // Warning Beacon ON
    } else {
        gpio_bank_[0].state = 0;
        gpio_bank_[1].state = 0;
        gpio_bank_[4].state = 0;
    }

    // Watchdog pin toggles
    gpio_bank_[2].state = watchdog_ok ? (1 - gpio_bank_[2].state) : 0;
}

bool HardwareModel::read_register(const std::string& dev_name, uint8_t reg_addr, uint16_t& out_val, std::string& out_name, std::string& out_unit, float& out_scaled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it_dev = devices_.find(dev_name);
    if (it_dev == devices_.end()) return false;

    auto it_reg = it_dev->second.registers.find(reg_addr);
    if (it_reg == it_dev->second.registers.end()) return false;

    out_val = it_reg->second.value;
    out_name = it_reg->second.name;
    out_unit = it_reg->second.unit;
    out_scaled = static_cast<float>(out_val) * it_reg->second.scaling_factor;
    return true;
}

bool HardwareModel::write_register(const std::string& dev_name, uint8_t reg_addr, uint16_t new_val) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it_dev = devices_.find(dev_name);
    if (it_dev == devices_.end()) return false;

    auto it_reg = it_dev->second.registers.find(reg_addr);
    if (it_reg == it_dev->second.registers.end()) return false;

    it_reg->second.value = new_val;
    return true;
}

bool HardwareModel::read_gpio(uint8_t pin, uint8_t& out_state, std::string& out_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gpio_bank_.find(pin);
    if (it == gpio_bank_.end()) return false;
    out_state = it->second.state;
    out_name = it->second.name;
    return true;
}

bool HardwareModel::write_gpio(uint8_t pin, uint8_t state) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gpio_bank_.find(pin);
    if (it == gpio_bank_.end()) return false;
    it->second.state = (state != 0) ? 1 : 0;
    return true;
}

std::vector<HardwareDevice> HardwareModel::get_all_devices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HardwareDevice> list;
    for (const auto& [name, dev] : devices_) {
        list.push_back(dev);
    }
    return list;
}

std::vector<GpioPin> HardwareModel::get_all_gpio() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GpioPin> list;
    for (const auto& [pin, g] : gpio_bank_) {
        list.push_back(g);
    }
    return list;
}

std::string HardwareModel::get_topology_tree() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "\r\nVIRTUAL HARDWARE BUS TOPOLOGY & PHYSICAL COMPONENT TREE:\r\n";

    for (const auto& [name, dev] : devices_) {
        oss << " ├── [" << dev.bus_type << " @ 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(dev.bus_address) << "] "
            << dev.name << " (" << dev.part_number << "): " << dev.description << "\r\n";
        for (const auto& [addr, reg] : dev.registers) {
            float scaled = static_cast<float>(reg.value) * reg.scaling_factor;
            oss << " │    ├── Reg 0x" << std::hex << std::setw(2) << static_cast<int>(addr)
                << " [" << std::dec << std::left << std::setw(14) << reg.name << "] = 0x"
                << std::hex << std::setw(4) << reg.value << " -> "
                << std::dec << std::fixed << std::setprecision(2) << scaled << " " << reg.unit
                << (reg.read_only ? " [RO]" : " [RW]") << "\r\n";
        }
    }

    oss << " └── [GPIO BANK A - Physical Relay & Interlock Lines]\r\n";
    for (const auto& [pin, g] : gpio_bank_) {
        oss << "      ├── PIN " << static_cast<int>(pin) << " (" << g.direction << "): "
            << std::setw(20) << std::left << g.name << " [STATE: "
            << (g.state ? "HIGH (1)" : "LOW  (0)") << "] - " << g.description << "\r\n";
    }

    return oss.str();
}

} // namespace industrial

#ifndef INDUSTRIAL_HARDWARE_ITEMS_HPP
#define INDUSTRIAL_HARDWARE_ITEMS_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>

namespace industrial {

// Register definition within an IC
struct HardwareRegister {
    uint8_t address;
    std::string name;
    uint16_t value;
    std::string description;
    std::string unit;
    float scaling_factor;
    bool read_only;
};

// Simulated Physical Integrated Circuit / Device
struct HardwareDevice {
    std::string name;
    std::string part_number;
    std::string bus_type; // "I2C-1", "SPI-0", "TIMER-2", "GPIO-A"
    uint8_t bus_address;
    std::string description;
    std::map<uint8_t, HardwareRegister> registers;
};

// Virtual GPIO Controller Bank
struct GpioPin {
    uint8_t pin_number;
    std::string name;
    std::string direction; // "IN", "OUT"
    uint8_t state;         // 0 (LOW), 1 (HIGH)
    std::string description;
};

class HardwareModel {
public:
    static HardwareModel& instance();

    void initialize();
    
    // Register access
    bool read_register(const std::string& dev_name, uint8_t reg_addr, uint16_t& out_val, std::string& out_name, std::string& out_unit, float& out_scaled);
    bool write_register(const std::string& dev_name, uint8_t reg_addr, uint16_t new_val);
    
    // GPIO access
    bool read_gpio(uint8_t pin, uint8_t& out_state, std::string& out_name);
    bool write_gpio(uint8_t pin, uint8_t state);

    // Sync live physical dynamics into registers
    void update_from_sensors(float temp, float current, float vibe, float rpm, ActuatorState act_state, bool watchdog_ok);

    std::vector<HardwareDevice> get_all_devices() const;
    std::vector<GpioPin> get_all_gpio() const;
    std::string get_topology_tree() const;

private:
    HardwareModel();
    ~HardwareModel() = default;

    mutable std::mutex mutex_;
    std::map<std::string, HardwareDevice> devices_;
    std::map<uint8_t, GpioPin> gpio_bank_;
    bool initialized_{false};
};

} // namespace industrial

#endif // INDUSTRIAL_HARDWARE_ITEMS_HPP

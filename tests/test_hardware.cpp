#include "hardware_items.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

namespace industrial {

void run_hardware_tests() {
    std::cout << "[TEST] Running Virtual Hardware Model & IC Register Tests..." << std::endl;

    auto& hw = HardwareModel::instance();

    // Test 1: Device enumeration
    auto devices = hw.get_all_devices();
    assert(devices.size() >= 4);
    std::cout << "  [PASS] Hardware devices enumerated: " << devices.size() << std::endl;

    // Test 2: Sensor update & register conversion for TMP117
    hw.update_from_sensors(45.5f, 5.0f, 2.5f, 1500.0f, ActuatorState::RUNNING, true);
    
    uint16_t raw_val = 0;
    std::string name, unit;
    float scaled = 0.0f;
    bool ok = hw.read_register("TMP117", 0x00, raw_val, name, unit, scaled);
    assert(ok);
    (void)ok;
    assert(name == "TEMP_RESULT");
    assert(std::fabs(scaled - 45.5f) < 0.1f);
    std::cout << "  [PASS] TMP117 register 0x00 scaled value: " << scaled << " C" << std::endl;

    // Test 3: INA219 current register
    ok = hw.read_register("INA219", 0x04, raw_val, name, unit, scaled);
    assert(ok);
    assert(name == "CURRENT_RAW");
    assert(std::fabs(scaled - 5.0f) < 0.1f);
    std::cout << "  [PASS] INA219 register 0x04 scaled value: " << scaled << " A" << std::endl;

    // Test 4: GPIO state transitions under Emergency Stop
    hw.update_from_sensors(95.0f, 18.0f, 8.5f, 0.0f, ActuatorState::EMERGENCY_STOP, false);
    uint8_t pwm_pin_state = 1;
    uint8_t estop_relay_state = 0;
    hw.read_gpio(0, pwm_pin_state, name); // Pin 0: MOTOR_PWM_ENABLE
    hw.read_gpio(1, estop_relay_state, name); // Pin 1: ESTOP_RELAY_TRIP

    assert(pwm_pin_state == 0); // Motor PWM must be killed (0)
    assert(estop_relay_state == 1); // ESTOP Contactor Relay tripped open (1)
    std::cout << "  [PASS] GPIO interlock: PWM killed (0) and ESTOP relay tripped (1) on safety fault" << std::endl;

    // Test 5: Register write to RW register
    bool write_ok = hw.write_register("TMP117", 0x02, 0x2D00); // 90°C high limit
    assert(write_ok);
    (void)write_ok;
    hw.read_register("TMP117", 0x02, raw_val, name, unit, scaled);
    assert(raw_val == 0x2D00);
    std::cout << "  [PASS] Register write verified on TMP117 0x02 THIGH_LIMIT" << std::endl;

    std::cout << "[TEST] All Hardware Model tests passed successfully!" << std::endl;
}

} // namespace industrial

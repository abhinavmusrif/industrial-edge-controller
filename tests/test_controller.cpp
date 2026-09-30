#include "controller.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace industrial;

void test_controller_nominal_transitions() {
    ActuatorController ctrl;
    assert(ctrl.get_state() == ActuatorState::OFF);

    ctrl.command_start();
    assert(ctrl.get_state() == ActuatorState::STARTING);

    std::map<SensorId, SensorReading> readings;
    std::map<SensorId, bool> staleness;
    SensorReading r;
    r.is_warning = false;
    r.is_critical = false;
    readings[SensorId::TEMPERATURE] = r;

    // Spin-up delay (500ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    ctrl.evaluate(readings, staleness, true, true);
    assert(ctrl.get_state() == ActuatorState::RUNNING);

    std::cout << "  [PASS] test_controller_nominal_transitions: OFF -> STARTING -> RUNNING sequence verified\n";
}

void test_controller_critical_trip() {
    ActuatorController ctrl;
    ctrl.command_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    std::map<SensorId, SensorReading> readings;
    std::map<SensorId, bool> staleness;
    SensorReading r;
    r.is_warning = true;
    r.is_critical = true; // Critical temperature excursion
    readings[SensorId::TEMPERATURE] = r;

    ctrl.evaluate(readings, staleness, true, true);
    assert(ctrl.get_state() == ActuatorState::EMERGENCY_STOP);

    // Verify it stays tripped even if reading lowers
    r.is_critical = false;
    r.is_warning = false;
    readings[SensorId::TEMPERATURE] = r;
    ctrl.evaluate(readings, staleness, true, true);
    assert(ctrl.get_state() == ActuatorState::EMERGENCY_STOP);

    // Verify reset clears trip
    ctrl.command_reset();
    assert(ctrl.get_state() == ActuatorState::OFF);

    std::cout << "  [PASS] test_controller_critical_trip: EMERGENCY_STOP interlock and manual reset verified\n";
}

void test_controller_hysteresis() {
    ActuatorController ctrl;
    ctrl.command_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    ctrl.set_hysteresis_ms(200); // Set short 200ms hysteresis for test

    std::map<SensorId, SensorReading> readings;
    std::map<SensorId, bool> staleness;
    SensorReading r;
    r.is_warning = true;
    r.is_critical = false;
    readings[SensorId::TEMPERATURE] = r;

    ctrl.evaluate(readings, staleness, true, true);
    assert(ctrl.get_state() == ActuatorState::WARNING);

    // Sensor reading recovers to normal
    r.is_warning = false;
    readings[SensorId::TEMPERATURE] = r;

    // Immediately evaluate: should still remain in WARNING due to hysteresis
    ctrl.evaluate(readings, staleness, true, true);
    assert(ctrl.get_state() == ActuatorState::WARNING);

    // Sleep past hysteresis window
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    ctrl.evaluate(readings, staleness, true, true);
    assert(ctrl.get_state() == ActuatorState::RUNNING);

    std::cout << "  [PASS] test_controller_hysteresis: state anti-chattering hysteresis verified\n";
}

void test_controller_safe_stop() {
    ActuatorController ctrl;
    ctrl.command_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    std::map<SensorId, SensorReading> readings;
    std::map<SensorId, bool> staleness;
    SensorReading r;
    r.is_warning = false;
    r.is_critical = false;
    readings[SensorId::TEMPERATURE] = r;

    ctrl.evaluate(readings, staleness, true, true);
    assert(ctrl.get_state() == ActuatorState::RUNNING);

    // Watchdog trips (watchdog_ok = false)
    ctrl.evaluate(readings, staleness, false, true);
    assert(ctrl.get_state() == ActuatorState::SAFE_STOP);

    std::cout << "  [PASS] test_controller_safe_stop: failsafe trip on watchdog/link loss verified\n";
}

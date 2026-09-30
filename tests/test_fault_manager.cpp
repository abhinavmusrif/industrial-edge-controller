#include "fault_manager.hpp"
#include "watchdog.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace industrial;

void test_fault_recording_and_clearing() {
    FaultManager fm;
    assert(!fm.has_active_fault());

    fm.record_fault(FaultCode::HIGH_TEMPERATURE, SensorId::TEMPERATURE, 92.4f, 85.0f, "EMERGENCY_STOP");
    assert(fm.has_active_fault());
    assert(fm.get_highest_severity_fault() == FaultCode::HIGH_TEMPERATURE);

    auto active = fm.get_active_faults();
    assert(active.size() == 1);
    assert(active[0].value == 92.4f);
    assert(active[0].threshold == 85.0f);

    fm.clear_fault(FaultCode::HIGH_TEMPERATURE);
    assert(!fm.has_active_fault());

    std::cout << "  [PASS] test_fault_recording_and_clearing: fault logging, tracking, and clearing verified\n";
}

void test_fault_listener_notification() {
    FaultManager fm;
    bool listener_called = false;
    FaultCode received_code = FaultCode::NONE;

    fm.register_listener([&](const FaultRecord& rec) {
        listener_called = true;
        received_code = rec.code;
    });

    fm.record_fault(FaultCode::HIGH_CURRENT, SensorId::CURRENT, 16.5f, 15.0f, "EMERGENCY_STOP");
    assert(listener_called);
    assert(received_code == FaultCode::HIGH_CURRENT);

    std::cout << "  [PASS] test_fault_listener_notification: asynchronous fault event listener verified\n";
}

void test_watchdog_timeout_behavior() {
    Watchdog wd(150); // 150 ms timeout
    assert(wd.is_healthy());

    bool callback_fired = false;
    wd.set_timeout_callback([&](uint64_t elapsed, uint64_t timeout) {
        (void)elapsed;
        (void)timeout;
        callback_fired = true;
    });

    // Pet watchdog
    wd.kick();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(wd.check());
    assert(!callback_fired);

    // Allow timeout to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(160));
    bool healthy = wd.check();
    (void)healthy;
    assert(!healthy);
    assert(callback_fired);

    // Recovery test
    wd.kick();
    assert(wd.is_healthy());

    std::cout << "  [PASS] test_watchdog_timeout_behavior: watchdog countdown, trip edge, and recovery verified\n";
}

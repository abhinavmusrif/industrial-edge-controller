#include "sensor.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace industrial;

void test_sensor_range() {
    SensorConfig cfg{
        SensorId::TEMPERATURE, "Temperature", "C",
        20.0f, 100.0f, 48.5f, 35.0f, 70.0f, 70.0f, 85.0f, 500
    };
    VirtualSensor sensor(cfg);

    for (int i = 0; i < 50; ++i) {
        auto r = sensor.sample();
        (void)r;
        assert(r.raw_value >= cfg.min_val && r.raw_value <= cfg.max_val);
        assert(r.filtered_value >= cfg.min_val && r.filtered_value <= cfg.max_val);
    }
    std::cout << "  [PASS] test_sensor_range: 50 samples verified within bounds [20, 100]\n";
}

void test_sensor_smooth_variation() {
    SensorConfig cfg{
        SensorId::CURRENT, "Current", "A",
        0.0f, 20.0f, 6.2f, 2.0f, 10.0f, 10.0f, 15.0f, 200
    };
    VirtualSensor sensor(cfg);

    float prev_val = sensor.sample().filtered_value;
    float max_delta = 0.0f;

    for (int i = 0; i < 30; ++i) {
        float curr = sensor.sample().filtered_value;
        float delta = std::abs(curr - prev_val);
        if (delta > max_delta) max_delta = delta;
        prev_val = curr;
    }
    // Delta between consecutive samples should be smooth (< 2.5A)
    assert(max_delta < 2.5f);
    std::cout << "  [PASS] test_sensor_smooth_variation: verified continuous dynamics (max delta: " << max_delta << ")\n";
}

void test_sensor_threshold_detection() {
    SensorConfig cfg{
        SensorId::TEMPERATURE, "Temperature", "C",
        20.0f, 100.0f, 48.5f, 35.0f, 70.0f, 70.0f, 85.0f, 500
    };
    VirtualSensor sensor(cfg);

    // 1. Nominal test
    sensor.inject_override(50.0f);
    auto r1 = sensor.sample();
    (void)r1;
    assert(!r1.is_warning && !r1.is_critical);

    // 2. Warning band test (75°C)
    sensor.inject_override(75.0f);
    auto r2 = sensor.sample();
    (void)r2;
    assert(r2.is_warning && !r2.is_critical);

    // 3. Critical band test (90°C)
    sensor.inject_override(90.0f);
    auto r3 = sensor.sample();
    (void)r3;
    assert(r3.is_critical && r3.is_warning);

    sensor.clear_override();
    std::cout << "  [PASS] test_sensor_threshold_detection: nominal, warning, and critical transitions verified\n";
}

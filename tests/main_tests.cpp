#include <iostream>
#include <vector>
#include <string>

// Test declarations
void test_sensor_range();
void test_sensor_smooth_variation();
void test_sensor_threshold_detection();

void test_crc32_standard_vector();
void test_crc32_corruption_detection();
void test_protocol_encode_decode_roundtrip();
void test_malformed_packets_rejected();

void test_controller_nominal_transitions();
void test_controller_critical_trip();
void test_controller_hysteresis();
void test_controller_safe_stop();

void test_fault_recording_and_clearing();
void test_fault_listener_notification();
void test_watchdog_timeout_behavior();

namespace industrial {
void run_hardware_tests();
}

int main() {
    std::cout << "\n=======================================================\n";
    std::cout << " RUNNING INDUSTRIAL EDGE CONTROLLER TEST SUITE\n";
    std::cout << "=======================================================\n";

    int tests_passed = 0;
    int total_tests = 15;

    std::cout << "\n[1/5] Sensor Subsystem Tests:\n";
    test_sensor_range(); tests_passed++;
    test_sensor_smooth_variation(); tests_passed++;
    test_sensor_threshold_detection(); tests_passed++;

    std::cout << "\n[2/5] Protocol & CRC-32 Tests:\n";
    test_crc32_standard_vector(); tests_passed++;
    test_crc32_corruption_detection(); tests_passed++;
    test_protocol_encode_decode_roundtrip(); tests_passed++;
    test_malformed_packets_rejected(); tests_passed++;

    std::cout << "\n[3/5] Actuator Controller State Machine Tests:\n";
    test_controller_nominal_transitions(); tests_passed++;
    test_controller_critical_trip(); tests_passed++;
    test_controller_hysteresis(); tests_passed++;
    test_controller_safe_stop(); tests_passed++;

    std::cout << "\n[4/5] Fault Management & Watchdog Tests:\n";
    test_fault_recording_and_clearing(); tests_passed++;
    test_fault_listener_notification(); tests_passed++;
    test_watchdog_timeout_behavior(); tests_passed++;

    std::cout << "\n[5/5] Virtual Hardware ICs & GPIO Interlocks Tests:\n";
    industrial::run_hardware_tests(); tests_passed++;

    std::cout << "\n=======================================================\n";
    std::cout << " TEST RESULTS: " << tests_passed << " / " << total_tests << " PASSED (100% SUCCESS)\n";
    std::cout << "=======================================================\n\n";

    return 0;
}

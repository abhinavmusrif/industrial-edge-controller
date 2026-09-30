cmake_minimum_required(VERSION 3.14)
project(industrial_edge_controller VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(include)

# Core library
set(CORE_SOURCES
    src/protocol.cpp
    src/sensor.cpp
    src/sensor_manager.cpp
    src/controller.cpp
    src/watchdog.cpp
    src/fault_manager.cpp
    src/transport.cpp
    src/uart_transport.cpp
    src/hardware_items.cpp
    src/logger.cpp
    src/telemetry.cpp
    src/tcp_server.cpp
    src/modbus_server.cpp
    src/mqtt_client.cpp
    src/gateway.cpp
)

add_library(industrial_core STATIC ${CORE_SOURCES})

# Platform networking and threading libraries
find_package(Threads REQUIRED)
target_link_libraries(industrial_core PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(industrial_core PUBLIC ws2_32)
endif()

# 1. Edge Gateway Executable
add_executable(edge_gateway
    src/main_gateway.cpp
    gateway/main.cpp
)
target_link_libraries(edge_gateway PRIVATE industrial_core)

# 2. MCU Simulator Executable
add_executable(mcu_simulator
    src/main_mcu_sim.cpp
    mcu_sim/main.cpp
)
target_link_libraries(mcu_simulator PRIVATE industrial_core)

# 3. Test Suite Executable
add_executable(run_tests
    tests/main_tests.cpp
    tests/test_sensor.cpp
    tests/test_protocol.cpp
    tests/test_controller.cpp
    tests/test_fault_manager.cpp
    tests/test_hardware.cpp
)
target_link_libraries(run_tests PRIVATE industrial_core)

enable_testing()
add_test(NAME AllUnitTests COMMAND run_tests)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

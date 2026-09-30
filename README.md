# Industrial Edge Controller & Digital Twin

[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-blue.svg)]()
[![Standard](https://img.shields.io/badge/C%2B%2B-17-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

> **IMPORTANT NOTICE:**  
> **This project is a software-only digital twin/simulation. No physical embedded hardware is claimed.**  
> The virtual sensors and MCU simulator emulate the operational dynamics, electrical envelopes, and serial communication links of industrial physical devices. The software architecture is designed using decoupled interfaces (`ITransport`) so that physical microcontrollers, RS-485 serial transceivers, and CAN-bus peripherals could directly replace the simulator in production.

---

## 1. Project Overview

The **Industrial Edge Controller & Digital Twin** is an end-to-end simulation of a mission-critical Linux-based industrial edge gateway paired with a microcontroller unit (MCU) sensor acquisition digital twin.

In modern industrial facilities (Industry 4.0, smart manufacturing, SCADA/DCS installations), edge gateways bridge low-level real-time sensor networks with supervisory SCADA systems and cloud telemetry pipelines. This project models that entire topology on a standard developer workstation without requiring physical hardware:

- **MCU Simulator:** Runs periodic deadline-scheduled sampling tasks (`steady_clock`), local digital filtering (moving average), safety threshold detection, and binary protocol framing with **CRC-32**.
- **Transport Abstraction:** Decoupled `ITransport` layer simulating serial/UART links over TCP localhost (port `9000`).
- **Linux Edge Gateway:** Central multi-threaded system coordinator with 6 concurrent POSIX threads managing transport stream synchronization, CRC validation, sensor staleness tracking, actuator safety state machine with hysteresis, hardware watchdog timer, structured logging, and dual SCADA/telemetry servers.
- **SCADA Interfaces:** Embedded **Modbus TCP Server** (port `1502`, holding registers `40001-40008`) and interactive **CLI TCP Monitor** (port `9100`).
- **Telemetry Pipeline:** Native **MQTT v3.1.1** telemetry client with automatic reconnect and JSON serialization.
- **Diagnostics & Fault Injection:** Interactive CLI tools and web-based SCADA dashboard with live fault simulation (`--temperature-high`, `--current-high`, `--vibration-high`, `--bad-crc`, `--sensor-timeout`, etc.).

---

## 2. System Architecture

```
                    ┌─────────────────────────────────┐
                    │     Virtual Sensors (C++)       │
                    │ Temperature | Current           │
                    │ Vibration   | Motor RPM         │
                    └────────────────┬────────────────┘
                                     │
                                     ▼
                    ┌─────────────────────────────────┐
                    │       MCU Simulator (C++)       │
                    │ • Periodic sampling (100-500ms) │
                    │ • Moving-average filtering      │
                    │ • Frame packaging + CRC-32      │
                    │ • Fault injection listener 9001 │
                    └────────────────┬────────────────┘
                                     │
                         ITransport / TCP:9000
                    (Simulated UART / Serial Bus)
                                     │
                                     ▼
                    ┌─────────────────────────────────┐
                    │    Linux Edge Gateway (C++)     │
                    │ ─────────────────────────────── │
                    │ Thread 1: MCU Comm & Framing    │
                    │ Thread 2: Staleness Monitor     │
                    │ Thread 3: Control Loop (50 Hz)  │
                    │ Thread 4: Telemetry & MQTT      │
                    │ Thread 5: Watchdog Countdown    │
                    │ Thread 6: CLI Monitor (TCP 9100)│
                    │ Thread 7: Modbus TCP (Port 1502)│
                    └───────┬─────────────────┬───────┘
                            │                 │
               Modbus TCP   │                 │ MQTT v3.1.1
               (Port 1502)  │                 │ (Port 1883)
                            ▼                 ▼
                 ┌──────────────────┐   ┌───────────────┐
                 │ SCADA / PLC Host │   │ Mosquitto     │
                 │ modbus_client.py │   │ MQTT Broker   │
                 └──────────────────┘   └───────┬───────┘
                                                │
                                                ▼
                                    ┌───────────────────────┐
                                    │ Web SCADA Dashboard   │
                                    │ (HTML5 / JS / Python) │
                                    │ http://localhost:8080 │
                                    └───────────────────────┘
```

---

## 3. Technology Stack

- **Core Application:** C++17
  - POSIX Threads / `std::thread`, `std::mutex`, `std::condition_variable`
  - High-resolution real-time clocks: `std::chrono::steady_clock`
  - Low-level network sockets: `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `select()` / `poll()`
  - Cross-platform portability layer supporting native Linux (GCC/Clang) and Windows (MinGW / MSVC)
- **SCADA Protocol:** Modbus TCP (MBAP Header + Function Code `0x03` Read Holding Registers)
- **Telemetry Protocol:** MQTT v3.1.1 (native packet serializer) + JSON payload formatting
- **Automation & Tools:** Python 3 (standard library socket / HTTP server, zero third-party dependencies required)
- **Build System:** CMake >= 3.14

---

## 4. Virtual Sensors & Physical Limits

| Sensor ID | Parameter | Nominal Range | Warning Band | Critical Limit | Sample Period | Engineering Unit |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|
| `1` | Temperature | 35.0 – 70.0 | 70.0 – 85.0 | > 85.0 | 500 ms | °C |
| `2` | Motor Current | 2.0 – 10.0 | 10.0 – 15.0 | > 15.0 | 200 ms | A |
| `3` | Vibration | 0.0 – 4.0 | 4.0 – 7.0 | > 7.0 | 100 ms | mm/s RMS |
| `4` | Motor Speed | 500 – 3000 | 2800 – 3200 | > 3200 | 100 ms | RPM |

*All sensor values use harmonic functions combined with continuous physical drift and deterministic pseudo-random variation. No pure white noise is used.*

---

## 5. Control State Machine

```
   [START]
      │
      ▼
   ┌──────┐  command_start()  ┌──────────┐  500ms nominal  ┌─────────┐
   │ OFF  ├──────────────────►│ STARTING ├────────────────►│ RUNNING │◄────┐
   └──────┘                   └────┬─────┘                 └────┬────┘     │
      ▲                            │                            │          │
      │                            │ Critical Sensor /          │ Elevated │ All Nominal &
      │ Manual                     │ Fault Detected             │ Reading  │ Hysteresis
      │ Reset                      │                            ▼          │ (2000 ms)
      │                            │                       ┌─────────┐     │
      │                            │                       │ WARNING ├─────┘
      │                            │                       └────┬────┘
      │                            │                            │
      │                            ▼                            ▼
      │                     ┌──────────────────────────────────────┐
      └─────────────────────┤ EMERGENCY_STOP / SAFE_STOP Interlock │
                            └──────────────────────────────────────┘
```

- **EMERGENCY_STOP:** Triggered immediately when any sensor exceeds its critical threshold (e.g. Temperature > 85.0°C). Latched until manual `RESET`.
- **SAFE_STOP:** Triggered when the Watchdog timer expires (> 2000ms heartbeat loss) or any sensor telemetry becomes stale (> 1500ms without update).
- **Anti-Chattering Hysteresis:** Prevents oscillating between `WARNING` and `RUNNING`. Sensor readings must stay below warning thresholds for at least 2.0 seconds continuously before clearing warning status.

---

## 6. Binary Wire Protocol & CRC-32

Sensor frames are packaged into deterministic binary packets:

```
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Magic 0     |   Magic 1     |    Version    |   Device ID   |
|    (0xAA)     |    (0x55)     |     (0x01)    |   (2 Bytes)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Msg Type    |               Sequence Number                 |
|   (1 Byte)    |                  (4 Bytes)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                       Timestamp (8 Bytes)                     +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        Payload Length         |    Payload Data (N Bytes)...  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           CRC-32                              |
|           (4 Bytes, IEEE 802.3 Polynomial 0xEDB88320)         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## 7. Modbus TCP Register Mapping

The gateway hosts a standard Modbus TCP server on port `1502` (Slave Unit ID `1`):

| Register | Address | Type | Scale | Description | Example |
|:---:|:---:|:---:|:---:|:---|:---|
| `40001` | `0x0000` | INT16 | x10 | Temperature (°C) | `502` -> `50.2 °C` |
| `40002` | `0x0001` | INT16 | x10 | Motor Current (A) | `66` -> `6.6 A` |
| `40003` | `0x0002` | INT16 | x10 | Vibration (mm/s RMS) | `20` -> `2.0 mm/s` |
| `40004` | `0x0003` | UINT16 | 1:1 | Motor Speed (RPM) | `1948 RPM` |
| `40005` | `0x0004` | UINT16 | Enum | Actuator State (0=OFF, 1=START, 2=RUN, 3=WARN, 4=ESTOP, 5=SAFE_STOP) | `2` (`RUNNING`) |
| `40006` | `0x0005` | UINT16 | Enum | Fault Code (0=NONE, 1=TEMP, 2=CURR, 3=VIBE, 4=STALE, 5=WATCHDOG, 6=CRC) | `0` (`NONE`) |
| `40007` | `0x0006` | UINT16 | Flag | Watchdog Status (1=OK, 0=EXPIRED) | `1` (`OK`) |
| `40008` | `0x0007` | UINT16 | Flag | MCU Transport Link (1=CONNECTED, 0=DISCONNECTED) | `1` (`CONNECTED`) |

---

## 8. Build & Quick Start

### Prerequisites
- CMake >= 3.14
- C++17 compiler (GCC, Clang, or MinGW/MSVC)
- Python 3.8+

### Build the Project
```bash
# On Linux / macOS / WSL:
./build.sh

# On Windows:
.\build.bat
```
*This verifies dependencies, compiles the static library and executables with full compiler warnings (`-Wall -Wextra -Wpedantic`), and automatically executes all 14 automated unit tests.*

### Run the One-Command Demo
```bash
# On Linux / macOS / WSL:
./run_demo.sh

# On Windows:
.\run_demo.bat   # or .\run_demo.ps1
```

### Stop the Demo
```bash
# On Linux / macOS / WSL:
./stop_demo.sh

# On Windows:
.\stop_demo.bat  # or .\stop_demo.ps1
```

---

## 9. Interactive Tools & Diagnostics

### 1. Web SCADA Dashboard
Open your web browser and navigate to:
**`http://localhost:8080`**
Features live gauges, actuator state indicators, real-time event logs, and one-click fault injection buttons.

### 2. CLI TCP Monitoring Interface (Port 9100)
Connect interactively using Netcat or the included Python tool:
```bash
# Interactive netcat session:
nc localhost 9100

# Or via Python monitor tool:
python tools/tcp_monitor.py --cmd STATUS
python tools/tcp_monitor.py --cmd SENSORS
python tools/tcp_monitor.py --cmd FAULTS
python tools/tcp_monitor.py --cmd JSON
```

### 3. Modbus TCP Client
Query industrial holding registers:
```bash
# Single query:
python tools/modbus_client.py --port 1502

# Continuous SCADA polling loop:
python tools/modbus_client.py --port 1502 --loop
```

### 4. Fault Injection Engine
Inject real-time dynamic hardware and transport faults without code changes:
```bash
# Inject over-temperature condition (forces 92.5°C -> triggers EMERGENCY_STOP):
python tools/fault_injector.py --temperature-high

# Inject over-current condition (forces 16.8A -> triggers EMERGENCY_STOP):
python tools/fault_injector.py --current-high

# Inject excessive vibration (forces 8.4 mm/s):
python tools/fault_injector.py --vibration-high

# Corrupt frame CRC-32 checksums:
python tools/fault_injector.py --bad-crc

# Pause transmissions for 4 seconds (triggers Watchdog timeout -> SAFE_STOP):
python tools/fault_injector.py --sensor-timeout

# Clear all injected faults and recover actuator:
python tools/fault_injector.py --reset
```

### 5. MQTT Telemetry Stream
```bash
python tools/mqtt_monitor.py --topic "industrial/device01/#"
```

---

## 10. Wireshark Network Inspection Guide

To demonstrate low-level network and systems knowledge during an interview:

1. Launch Wireshark and select the **Loopback adapter** (`lo` on Linux, `Npcap Loopback Adapter` on Windows).
2. Enter one of the following display filters in the filter bar:
   - `tcp.port == 9000` (MCU-to-Gateway binary protocol frames)
   - `tcp.port == 9100` (CLI Monitor commands and responses)
   - `tcp.port == 1502` (Modbus TCP query and response packets)
   - `mqtt` or `tcp.port == 1883` (MQTT telemetry traffic)
3. Start capturing packets.
4. Execute `python tools/fault_injector.py --bad-crc`.
5. In Wireshark, examine the raw payload of port 9000:
   - Notice the sync bytes `0xAA 0x55`.
   - Observe the 4-byte CRC-32 at the tail of the packet.
   - Note the Gateway immediately stops processing the corrupted frame and logs the event!

---

## 11. Automated Test Suite

Run the unit tests directly:
```bash
./build/bin/run_tests
```
The test suite validates 14 core functional units:
1. `test_sensor_range`: 50 samples strictly within physical envelopes.
2. `test_sensor_smooth_variation`: Verifies continuous dynamics and absence of white-noise spikes.
3. `test_sensor_threshold_detection`: Nominal, warning, and critical state checks.
4. `test_crc32_standard_vector`: Exact match with IEEE 802.3 test vector (`0xCBF43926`).
5. `test_crc32_corruption_detection`: Verifies single-bit corruption detection.
6. `test_protocol_encode_decode_roundtrip`: Complete binary frame serialization/deserialization.
7. `test_malformed_packets_rejected`: Rejection of corrupted CRCs, short frames, and stream resynchronization.
8. `test_controller_nominal_transitions`: State progression `OFF` -> `STARTING` -> `RUNNING`.
9. `test_controller_critical_trip`: Immediate `EMERGENCY_STOP` trip and latching.
10. `test_controller_hysteresis`: State stabilization preventing boundary chattering.
11. `test_controller_safe_stop`: Watchdog/link loss trip.
12. `test_fault_recording_and_clearing`: Fault logging, prioritization, and lifecycle.
13. `test_fault_listener_notification`: Asynchronous fault event listener dispatch.
14. `test_watchdog_timeout_behavior`: Countdown, trip edge-triggering, and kick recovery.

---

## 12. Interview Talking Points & Design Rationale

1. **Hardware / Software Decoupling:**  
   The project implements an explicit `ITransport` abstraction. Higher-level logic knows only `send_bytes()` and `receive_bytes()`. In a physical hardware setup, `TcpTransport` is replaced with `UartTransport` (using Linux `/dev/ttyUSB0` or `termios`) or `CanTransport` (using `SocketCAN`), without modifying a single line of sensor processing or control logic.
2. **Periodic Task Scheduling vs `sleep_for`:**  
   Periodic sampling uses deadline-based scheduling (`next_deadline += period; sleep_until(next_deadline)`). Simple `sleep_for(period)` accumulates execution drift over time; deadline scheduling eliminates cumulative jitter.
3. **Fail-Safe Industrial State Machine:**  
   Industrial control requires predictable failure modes. The controller enforces safety interlocks, prevents automated restart after critical trips, and incorporates hysteresis to avoid thrashing on physical boundary values.
4. **Structured Binary Protocol over JSON on Wire:**  
   While JSON is great for cloud dashboards, embedded microcontroller communication requires compact, low-overhead binary framing with hardware CRC verification to withstand electrical noise and frame synchronization loss.
5. **Zero-Dependency Architecture:**  
   The core C++ code and Python tools are implemented purely on native system and POSIX/WinSock APIs, ensuring immediate compilation and execution on any developer system without fragile package manager dependencies.

# 5-Minute Live Technical Interview Demo Script

This script provides an exact, step-by-step walkthrough to demonstrate the **Industrial Edge Controller & Digital Twin** during a technical interview.

---

## Pre-Demo Checklist
- Terminal 1: Workspace root directory
- Terminal 2: Ready for monitoring tools
- Web Browser: Open at `http://localhost:8080` (or ready to open)

---

## Minute 1: System Launch & Architecture Overview

### Step 1: Start the Simulation
In Terminal 1:
```bash
./run_demo.sh     # Linux / macOS
# Or on Windows:
.\run_demo.bat
```

### Talking Point:
> *"I built a software digital twin modeling a two-tier industrial edge architecture: an embedded MCU acquiring real-time sensors, communicating across an abstract serial transport link to a multi-threaded Linux edge gateway. The gateway exposes Modbus TCP for SCADA systems, MQTT for cloud telemetry, an interactive CLI monitor, and runs a fail-safe state machine with a hardware watchdog."*

---

## Minute 2: Live SCADA & Protocol Demonstration

### Step 2: Show Web Dashboard
Open `http://localhost:8080` in your browser.
- Point to the **Simulation / Digital Twin banner**.
- Show the live sensor dials: Temperature (~50°C), Current (~6.6A), Vibration (~2.0 mm/s), RPM (~1950 RPM).
- Highlight the **Actuator State: RUNNING** and **Watchdog: OK**.

### Step 3: Show Modbus TCP SCADA Registers
In Terminal 2:
```bash
python tools/modbus_client.py --port 1502
```
### Talking Point:
> *"Here we are querying holding registers 40001 through 40008 using raw Modbus TCP MBAP sockets. This is standard Modbus Application Protocol, meaning any real industrial PLC, Siemens WinCC, or Ignition SCADA server can directly query our gateway."*

### Step 4: Show CLI TCP Monitoring Server
In Terminal 2:
```bash
python tools/tcp_monitor.py --cmd STATUS
python tools/tcp_monitor.py --cmd SENSORS
```
### Talking Point:
> *"The gateway provides an out-of-band CLI interface on port 9100. It shows filtered vs raw sensor readings and timing stats, designed for field technicians troubleshooting over SSH or local serial console."*

---

## Minute 3: Real-Time Network & Wire Inspection

### Step 5: Wireshark Inspection (Optional / Talking Point)
- Point out that Wireshark on loopback filtering for `tcp.port == 9000` shows binary frames with magic bytes `0xAA 0x55`, 64-bit millisecond timestamps, and a 32-bit CRC.
- Point out `tcp.port == 1502` displays standard Modbus TCP frames.

### Step 6: Gateway Structured Logging
In Terminal 2:
```bash
tail -n 15 logs/gateway.log
# Or on Windows PowerShell:
Get-Content logs/gateway.log -Tail 15
```
### Talking Point:
> *"Notice the structured log entries formatted with ISO-8601 millisecond timestamps and severity levels (INFO, WARN, CRIT). Every thread logs safely through a centralized, thread-safe logger."*

---

## Minute 4: Dynamic Fault Injection & Safety Interlocks

### Step 7: Inject Over-Temperature Fault
In Terminal 2:
```bash
python tools/fault_injector.py --temperature-high
```
Watch the console and the Web Dashboard at `http://localhost:8080`:
1. Sensor temperature rises smoothly through the moving-average filter.
2. Crosses warning threshold (70°C): Controller state enters **`WARNING`**.
3. Crosses critical threshold (85°C): Controller immediately trips into **`EMERGENCY_STOP`**!
4. Query CLI faults:
```bash
python tools/tcp_monitor.py --cmd FAULTS
```
Output:
`CRITICAL: TEMPERATURE = 92.5 (Threshold = 85.0), Action = EMERGENCY_STOP`

### Step 8: Clear Fault & Demonstrate Hysteresis Recovery
In Terminal 2:
```bash
python tools/fault_injector.py --reset
python tools/tcp_monitor.py --cmd START
```
### Talking Point:
> *"Notice the actuator doesn't immediately chatter back into RUNNING. Our state machine enforces an anti-chattering hysteresis timer: sensor values must stabilize in the nominal band for 2.0 seconds before the warning condition is officially cleared."*

---

## Minute 5: Watchdog Timeout & Hardware Migration

### Step 9: Simulate MCU Crash / Serial Link Loss
In Terminal 2:
```bash
python tools/fault_injector.py --sensor-timeout
```
Observe the reaction after 2 seconds:
1. Watchdog timer expires (> 2000 ms silence).
2. Controller trips into **`SAFE_STOP`**!
3. Active faults indicate `Watchdog heartbeat missing` and `Sensor STALE`.

### Step 10: Conclusion & Hardware Migration Answer
### Talking Point:
> *"To transition this from a digital twin to physical hardware, we simply replace `TcpTransport` with a `UartTransport` using Linux `/dev/ttyUSB0` or `CanTransport` using `SocketCAN`. The sensor acquisition and framing code compiles directly onto bare-metal firmware (like STM32). The Linux gateway, state machine, safety interlocks, and SCADA servers remain completely untouched."*

### Step 11: Clean Shutdown
In Terminal 1:
```bash
./stop_demo.sh    # Or .\stop_demo.bat
```

# Industrial Edge Controller & Digital Twin: System Architecture

## 1. Executive Summary

This document details the software architecture, concurrency model, data flow, failure modes, and hardware-readiness of the **Industrial Edge Controller & Digital Twin**. 

The system implements a classic two-tier industrial computing architecture:
1. **Tier 1: Embedded Field Device (MCU Simulator):** Fast, deterministic sensor acquisition, local signal conditioning, and compact binary framing.
2. **Tier 2: Linux Edge Gateway:** Multi-threaded telemetry processing, supervisory control logic, fault management, watchdog supervision, and northbound SCADA/Cloud interfaces (Modbus TCP, MQTT, CLI).

---

## 2. Concurrency & Threading Model

The Linux Edge Gateway avoids global mutable state and uncontrolled thread creation. All background worker threads are encapsulated within the `LinuxGateway` class using RAII patterns.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Linux Edge Gateway                              │
├────────────────────────────────────────────────────────────────────────┤
│ Thread 1: MCU Communication Worker (TCP:9000)                          │
│   - Synchronizes binary stream on 0xAA 0x55 magic bytes                │
│   - Validates CRC-32 checksums on incoming frames                      │
│   - Discards corrupted packets and records CRC faults                  │
│   - Kicks hardware Watchdog upon receiving valid frame                 │
├────────────────────────────────────────────────────────────────────────┤
│ Thread 2: Sensor Health Monitor (100 ms loop)                          │
│   - Evaluates staleness timer: (now - last_update_ms > 1500 ms)        │
│   - Flags individual stale sensors and raises SENSOR_STALE faults       │
├────────────────────────────────────────────────────────────────────────┤
│ Thread 3: Real-Time Control Loop (50 Hz / 20 ms)                       │
│   - Executes actuator state machine (OFF -> START -> RUN -> ESTOP)     │
│   - Enforces safety interlocks and trip conditions                     │
│   - Manages recovery hysteresis (2000 ms stability window)             │
├────────────────────────────────────────────────────────────────────────┤
│ Thread 4: Telemetry Pipeline (500 ms interval)                         │
│   - Aggregates latest filtered sensor readings and status              │
│   - Formats standard JSON telemetry messages                           │
│   - Publishes to MQTT topics with auto-reconnection backoff            │
├────────────────────────────────────────────────────────────────────────┤
│ Thread 5: Hardware Watchdog Supervisor (100 ms check)                  │
│   - Monitors time elapsed since last MCU kick                          │
│   - Trips SAFE_STOP if elapsed > watchdog_timeout_ms (2000 ms)         │
├────────────────────────────────────────────────────────────────────────┤
│ Thread 6: CLI Monitoring Server (TCP:9100)                             │
│   - Accepts operator interactive netcat connections                    │
│   - Parses STATUS, SENSORS, FAULTS, ACTUATOR, JSON commands            │
├────────────────────────────────────────────────────────────────────────┤
│ Thread 7: Modbus TCP SCADA Server (TCP:1502)                           │
│   - Implements standard Modbus Application Protocol (MBAP)             │
│   - Serves holding registers 40001 - 40008 to external SCADA / PLCs    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Data Flow & Communication Pipeline

```
  [Virtual Sensors]
         │ (100-500 ms periodic deadline tasks)
         ▼
  [Moving Average Filter]
         │
         ▼
  [Local Threshold Checks]
         │
         ▼
  [Binary Frame Serializer]
         │ (Calculates IEEE 802.3 CRC-32)
         ▼
  [ITransport / TcpTransport] ─── Localhost TCP:9000 ───► [Linux Edge Gateway]
                                                                │
                                    ┌───────────────────────────┴───────────────────────────┐
                                    ▼                                                       ▼
                            [CRC-32 Validated]                                      [CRC-32 Corrupted]
                                    │                                                       │
                                    ├──────────────────────────┐                            ▼
                                    ▼                          ▼                    [Reject Frame]
                             [Kick Watchdog]           [Update Sensor Table]        [Record Fault]
                                                               │
                                                               ▼
                                                      [Control Loop 50Hz]
                                                               │
                                            ┌──────────────────┴──────────────────┐
                                            ▼                                     ▼
                                      [Nominal Range]                       [Critical / Stale]
                                            │                                     │
                                            ▼                                     ▼
                                    [Actuator RUNNING]                  [EMERGENCY_STOP / SAFE_STOP]
                                            │                                     │
                                            └──────────────────┬──────────────────┘
                                                               │
                                            ┌──────────────────┴──────────────────┐
                                            ▼                                     ▼
                                     [Modbus TCP:1502]                     [MQTT / JSON]
                                    (Holding Regs 40001+)                (Telemetry Topics)
```

---

## 4. Hardware Emulation & Integrated Circuits Model

To bridge the gap between pure data simulation and physical embedded hardware, the system includes a high-fidelity **Virtual Hardware Registry (`HardwareModel`)** modeling real off-the-shelf industrial components:

1. **TMP117AIDRVR (Texas Instruments - I2C Address `0x48`):**
   - High-precision ±0.1°C temperature sensor with 16-bit ADC.
   - Registers: `TEMP_RESULT` (`0x00`, 7.8125 m°C/LSB), `CONFIG` (`0x01`), `THIGH_LIMIT` (`0x02`), `TLOW_LIMIT` (`0x03`).
2. **INA219BIDR (Texas Instruments - I2C Address `0x40`):**
   - Bi-directional current shunt and power monitor.
   - Registers: `CONFIG` (`0x00`), `SHUNT_VOLTAGE` (`0x01`, 10 µV/LSB), `BUS_VOLTAGE` (`0x02`, 4 mV/LSB = 24.0V), `CURRENT_RAW` (`0x04`, 10 mA/LSB).
3. **MPU-6050 (InvenSense - I2C Address `0x68`):**
   - Triple-axis MEMS accelerometer with onboard Digital Motion Processor.
   - Registers: `ACCEL_X/Y/Z` (`0x3B-0x3F`), `VIBE_RMS` (`0x41`, 10 µm/s / LSB), `WHO_AM_I` (`0x75` = `0x68`).
4. **TIM-2 Optical Shaft Quadrature Encoder:**
   - 32-bit hardware timer counter coupled to motor shaft.
   - Registers: `TIM_CNT` (`0x00`), `SPEED_RPM` (`0x04`), `TIM_ARR` (`0x08`).
5. **Virtual GPIO Bank A (Physical Safety Interlocks):**
   - `PIN 0`: `MOTOR_PWM_ENABLE` (Gate drive line - killed on fault)
   - `PIN 1`: `ESTOP_RELAY_TRIP` (Contactor relay - trips OPEN on fault)
   - `PIN 2`: `WATCHDOG_WDI` (Hardware watchdog heartbeat toggle)
   - `PIN 3`: `OPERATOR_RESET_PB` (Pushbutton reset interlock)
   - `PIN 4`: `WARNING_BEACON_LED` (Control cabinet yellow warning tower)

---

## 5. Drop-in Physical Hardware Transports

The transport abstraction layer (`ITransport`) decouples the protocol, CRC validation, and control logic from the physical medium:

```cpp
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool connect_endpoint() = 0;
    virtual void disconnect_endpoint() = 0;
    virtual bool is_connected() const = 0;
    virtual int send_bytes(const uint8_t* data, size_t len) = 0;
    virtual int receive_bytes(uint8_t* buffer, size_t max_len, int timeout_ms = -1) = 0;
    virtual std::string get_transport_name() const = 0;
};
```

1. **`TcpTransport`:** Implements virtual localhost link on port `9000` for simulation, testing, and CI/CD pipelines.
2. **`UartTransport`:** Fully implemented drop-in production serial driver supporting POSIX `termios` (`/dev/ttyUSB0`, `/dev/ttyS0`, `/dev/pts/X`) and Windows COM ports (`\\\\.\\COM1-COM256`) with configurable baud rates (9600 to 230400), 8N1 framing, and non-blocking poll timeouts.
3. **`CanTransport`:** Extensible for Linux SocketCAN (`AF_CAN`, `CAN_RAW`).

---

## 6. Interactive Linux Virtual Hardware Lab & 3D Digital Twin

1. **Linux Hardware Lab (`tools/hardware_lab.py`):**
   - An interactive embedded Linux shell (`root@industrial-edge:/sys/kernel/debug# `)
   - Commands: `lsdev` (probe I2C/Timer/GPIO topology), `status`, `sensors`, `gpio`, `faults`, `inject`, `sniff` (live binary frame packet sniffer decoding IEEE 802.3 CRC-32 on the wire), and `dmesg`.
2. **Three.js 3D WebGL Digital Twin:**
   - Real-time 3D rendered AC induction motor with dynamic rotor rotation matched to live encoder RPM.
   - Dynamic stator heat-map shifting from cold blue (`35°C`) to nominal green (`55°C`), warning yellow (`75°C`), and critical red (`85°C+`).
   - High-frequency displacement vertex shaking mimicking live accelerometer vibration.
   - DIN-rail Edge Controller with live pulsing watchdog, MCU link, and fault beacon LEDs.


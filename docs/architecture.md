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

## 4. Hardware Replacement Architecture

In production embedded environments, physical sensors and microcontrollers communicate via fieldbuses rather than localhost TCP. The code is architected to facilitate this migration:

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

To deploy with real hardware:
1. **RS-485 / Serial UART:** Implement `UartTransport : public ITransport` using POSIX `open("/dev/ttyUSB0", O_RDWR | O_NOCTTY)` and `tcsetattr()`.
2. **CAN Bus:** Implement `CanTransport : public ITransport` using Linux `SocketCAN` (`AF_CAN`, `CAN_RAW`).
3. **Firmware:** Compile the sensor sampling logic and `Protocol::serialize_frame()` onto an STM32, ESP32, or NXP i.MX RT microcontroller.

The higher-level gateway code, control loop, Modbus server, and telemetry pipelines remain 100% untouched.

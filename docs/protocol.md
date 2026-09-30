# Industrial Edge Controller Binary Protocol Specification

## 1. Frame Layout

All multi-byte fields are transmitted in **Network Byte Order (Big-Endian)**.

| Field Offset | Field Name | Type | Size (Bytes) | Description |
|:---:|:---|:---:|:---:|:---|
| `0` | `magic0` | `uint8_t` | 1 | Sync magic byte 0: `0xAA` |
| `1` | `magic1` | `uint8_t` | 1 | Sync magic byte 1: `0x55` |
| `2` | `version` | `uint8_t` | 1 | Protocol version (`0x01`) |
| `3..4` | `device_id` | `uint16_t` | 2 | Originating Device ID |
| `5` | `message_type` | `uint8_t` | 1 | Frame message type code |
| `6..9` | `sequence` | `uint32_t` | 4 | Monotonically increasing packet counter |
| `10..17` | `timestamp` | `uint64_t` | 8 | Milliseconds since Unix epoch |
| `18..19` | `payload_length` | `uint16_t` | 2 | Length of payload $N$ ($0 \le N \le 1024$) |
| `20..(20+N-1)` | `payload` | `uint8_t[]` | $N$ | Application payload data |
| `(20+N)..(23+N)`| `crc` | `uint32_t` | 4 | CRC-32 over bytes `[0 .. (20+N-1)]` |

- **Header Size:** 20 bytes
- **Checksum Size:** 4 bytes
- **Minimum Frame Size:** 24 bytes ($N = 0$)
- **Maximum Frame Size:** 1048 bytes ($N = 1024$)

---

## 2. Message Types

| Value | Identifier | Description |
|:---:|:---|:---|
| `0x01` | `SENSOR_DATA` | Periodic sensor acquisition sample |
| `0x02` | `HEARTBEAT` | Keep-alive heartbeat frame (payload length = 0) |
| `0x03` | `FAULT_ALERT` | Microcontroller-detected local hardware fault |
| `0x04` | `COMMAND` | Downlink supervisory control directive |
| `0x05` | `ACK` | Command acknowledgement response |

---

## 3. Sensor Data Payload Structure

When `message_type == 0x01` (`SENSOR_DATA`), the payload is packed as follows:

```cpp
#pragma pack(push, 1)
struct SensorPayload {
    uint8_t sensor_id;      // 1=Temperature, 2=Current, 3=Vibration, 4=RPM
    float raw_value;        // IEEE 754 32-bit single-precision float
    float filtered_value;   // Output of moving-average filter
    uint8_t status_flags;   // Bit 0: Warning, Bit 1: Critical
};
#pragma pack(pop)
```

Total payload size = 10 bytes.

---

## 4. CRC-32 Specification

- **Algorithm:** CRC-32 / ISO-HDLC / IEEE 802.3
- **Polynomial:** `0x04C11DB7` (Normal), `0xEDB88320` (Reflected)
- **Initial Value:** `0xFFFFFFFF`
- **Final XOR:** `0xFFFFFFFF`
- **Standard Test Vector:** ASCII string `"123456789"` yields `0xCBF43926`.

Checksum calculation spans the entire frame up to the payload tail (`magic0` through the last byte of `payload`).

---

## 5. Framing & Resynchronization Strategy

Serial streams are vulnerable to bit slips, electrical glitches, and fragmented delivery. The deserializer employs a sliding sync-window algorithm:
1. Buffers incoming stream data.
2. Scans for the two-byte delimiter `0xAA 0x55`.
3. If not found, unaligned leading bytes are discarded.
4. Once sync is found, verifies that at least 20 header bytes are available.
5. Inspects `payload_length`. If $N > 1024$, frame is rejected as corrupt and synchronization restarts.
6. Once the full frame ($20 + N + 4$) is buffered, calculates CRC-32.
7. If CRC matches, frame is delivered to consumer; if CRC fails, the header is discarded and the parser immediately searches for the next sync sequence.

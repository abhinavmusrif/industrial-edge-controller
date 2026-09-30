#ifndef INDUSTRIAL_PROTOCOL_HPP
#define INDUSTRIAL_PROTOCOL_HPP

#include "common.hpp"
#include <vector>
#include <cstdint>
#include <string>

namespace industrial {

#pragma pack(push, 1)

constexpr uint8_t PROTOCOL_MAGIC_0 = 0xAA;
constexpr uint8_t PROTOCOL_MAGIC_1 = 0x55;
constexpr uint8_t PROTOCOL_CURRENT_VERSION = 1;
constexpr size_t HEADER_SIZE = 20; // Magic(2) + Ver(1) + DevId(2) + MsgType(1) + Seq(4) + Ts(8) + Len(2)
constexpr size_t CRC_SIZE = 4;
constexpr size_t MIN_FRAME_SIZE = HEADER_SIZE + CRC_SIZE; // 24 bytes
constexpr size_t MAX_PAYLOAD_SIZE = 1024;
constexpr size_t MAX_FRAME_SIZE = MIN_FRAME_SIZE + MAX_PAYLOAD_SIZE;

enum class MessageType : uint8_t {
    SENSOR_DATA = 0x01,
    HEARTBEAT   = 0x02,
    FAULT_ALERT = 0x03,
    COMMAND     = 0x04,
    ACK         = 0x05
};

struct SensorPayload {
    uint8_t sensor_id;
    float raw_value;
    float filtered_value;
    uint8_t status_flags; // bit 0: warning, bit 1: critical
};

struct Frame {
    uint8_t magic0{PROTOCOL_MAGIC_0};
    uint8_t magic1{PROTOCOL_MAGIC_1};
    uint8_t version{PROTOCOL_CURRENT_VERSION};
    uint16_t device_id{1};
    uint8_t message_type{static_cast<uint8_t>(MessageType::SENSOR_DATA)};
    uint32_t sequence{0};
    uint64_t timestamp{0};
    uint16_t payload_length{0};
    std::vector<uint8_t> payload;
    uint32_t crc{0};
};

#pragma pack(pop)

enum class DecodeResult {
    SUCCESS,
    NEED_MORE_DATA,
    CORRUPTED_CRC,
    INVALID_HEADER,
    PAYLOAD_TOO_LARGE
};

class Protocol {
public:
    static uint32_t calculate_crc32(const uint8_t* data, size_t length);
    static std::vector<uint8_t> serialize_frame(const Frame& frame, bool corrupt_crc = false);
    static DecodeResult deserialize_frame(std::vector<uint8_t>& rx_buffer, Frame& out_frame);
    
    static Frame create_sensor_frame(uint16_t device_id, uint32_t sequence,
                                     uint8_t sensor_id, float raw_val, float filtered_val,
                                     uint8_t status_flags);
    
    static Frame create_heartbeat_frame(uint16_t device_id, uint32_t sequence);
    
    static bool parse_sensor_payload(const std::vector<uint8_t>& payload, SensorPayload& out_sensor);
};

} // namespace industrial

#endif // INDUSTRIAL_PROTOCOL_HPP

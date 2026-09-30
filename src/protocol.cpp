#include "protocol.hpp"
#include <cstring>

namespace industrial {

uint32_t Protocol::calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320U & (-(static_cast<int32_t>(crc & 1U))));
        }
    }
    return ~crc;
}

std::vector<uint8_t> Protocol::serialize_frame(const Frame& frame, bool corrupt_crc) {
    std::vector<uint8_t> buffer;
    uint16_t payload_len = static_cast<uint16_t>(frame.payload.size());
    buffer.reserve(HEADER_SIZE + payload_len + CRC_SIZE);

    // 1. Magic (2 bytes)
    buffer.push_back(frame.magic0);
    buffer.push_back(frame.magic1);

    // 2. Version (1 byte)
    buffer.push_back(frame.version);

    // 3. Device ID (2 bytes, big-endian)
    buffer.push_back(static_cast<uint8_t>((frame.device_id >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(frame.device_id & 0xFF));

    // 4. Message Type (1 byte)
    buffer.push_back(frame.message_type);

    // 5. Sequence (4 bytes, big-endian)
    buffer.push_back(static_cast<uint8_t>((frame.sequence >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((frame.sequence >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((frame.sequence >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(frame.sequence & 0xFF));

    // 6. Timestamp (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        buffer.push_back(static_cast<uint8_t>((frame.timestamp >> (i * 8)) & 0xFF));
    }

    // 7. Payload Length (2 bytes, big-endian)
    buffer.push_back(static_cast<uint8_t>((payload_len >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(payload_len & 0xFF));

    // 8. Payload (N bytes)
    buffer.insert(buffer.end(), frame.payload.begin(), frame.payload.end());

    // 9. CRC-32 (4 bytes over Header + Payload)
    uint32_t crc = calculate_crc32(buffer.data(), buffer.size());
    if (corrupt_crc) {
        crc ^= 0xDEADBEEF; // Deliberate fault injection
    }

    buffer.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(crc & 0xFF));

    return buffer;
}

DecodeResult Protocol::deserialize_frame(std::vector<uint8_t>& rx_buffer, Frame& out_frame) {
    while (true) {
        if (rx_buffer.size() < MIN_FRAME_SIZE) {
            return DecodeResult::NEED_MORE_DATA;
        }

        // Search for sync magic bytes
        size_t sync_idx = 0;
        bool found_sync = false;
        for (; sync_idx + 1 < rx_buffer.size(); ++sync_idx) {
            if (rx_buffer[sync_idx] == PROTOCOL_MAGIC_0 && rx_buffer[sync_idx + 1] == PROTOCOL_MAGIC_1) {
                found_sync = true;
                break;
            }
        }

        if (!found_sync) {
            // Discard unaligned bytes except the last byte (which might be first magic byte)
            if (!rx_buffer.empty() && rx_buffer.back() == PROTOCOL_MAGIC_0) {
                rx_buffer.erase(rx_buffer.begin(), rx_buffer.end() - 1);
            } else {
                rx_buffer.clear();
            }
            return DecodeResult::NEED_MORE_DATA;
        }

        if (sync_idx > 0) {
            rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + sync_idx);
        }

        if (rx_buffer.size() < MIN_FRAME_SIZE) {
            return DecodeResult::NEED_MORE_DATA;
        }

        // Read payload length (bytes 18 and 19)
        uint16_t payload_len = (static_cast<uint16_t>(rx_buffer[18]) << 8) |
                                static_cast<uint16_t>(rx_buffer[19]);

        if (payload_len > MAX_PAYLOAD_SIZE) {
            // Invalid length, drop first byte and re-sync
            rx_buffer.erase(rx_buffer.begin());
            continue;
        }

        size_t total_expected_frame_size = HEADER_SIZE + payload_len + CRC_SIZE;
        if (rx_buffer.size() < total_expected_frame_size) {
            return DecodeResult::NEED_MORE_DATA;
        }

        // Validate CRC
        size_t crc_offset = HEADER_SIZE + payload_len;
        uint32_t expected_crc =
            (static_cast<uint32_t>(rx_buffer[crc_offset]) << 24) |
            (static_cast<uint32_t>(rx_buffer[crc_offset + 1]) << 16) |
            (static_cast<uint32_t>(rx_buffer[crc_offset + 2]) << 8) |
            static_cast<uint32_t>(rx_buffer[crc_offset + 3]);

        uint32_t calculated_crc = calculate_crc32(rx_buffer.data(), crc_offset);
        if (calculated_crc != expected_crc) {
            // CRC failure! Consume the corrupted header and try to recover next frame
            rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + 2);
            return DecodeResult::CORRUPTED_CRC;
        }

        // Successfully decoded! Populate out_frame
        out_frame.magic0 = rx_buffer[0];
        out_frame.magic1 = rx_buffer[1];
        out_frame.version = rx_buffer[2];
        out_frame.device_id = (static_cast<uint16_t>(rx_buffer[3]) << 8) | static_cast<uint16_t>(rx_buffer[4]);
        out_frame.message_type = rx_buffer[5];
        out_frame.sequence = (static_cast<uint32_t>(rx_buffer[6]) << 24) |
                             (static_cast<uint32_t>(rx_buffer[7]) << 16) |
                             (static_cast<uint32_t>(rx_buffer[8]) << 8) |
                             static_cast<uint32_t>(rx_buffer[9]);

        out_frame.timestamp = 0;
        for (int i = 0; i < 8; ++i) {
            out_frame.timestamp = (out_frame.timestamp << 8) | rx_buffer[10 + i];
        }

        out_frame.payload_length = payload_len;
        out_frame.payload.assign(rx_buffer.begin() + HEADER_SIZE,
                                 rx_buffer.begin() + HEADER_SIZE + payload_len);
        out_frame.crc = expected_crc;

        // Erase decoded frame from rx_buffer
        rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + total_expected_frame_size);
        return DecodeResult::SUCCESS;
    }
}

Frame Protocol::create_sensor_frame(uint16_t device_id, uint32_t sequence,
                                    uint8_t sensor_id, float raw_val, float filtered_val,
                                    uint8_t status_flags) {
    Frame f;
    f.device_id = device_id;
    f.message_type = static_cast<uint8_t>(MessageType::SENSOR_DATA);
    f.sequence = sequence;
    f.timestamp = get_current_time_ms();
    
    f.payload.resize(sizeof(SensorPayload));
    SensorPayload sp;
    sp.sensor_id = sensor_id;
    sp.raw_value = raw_val;
    sp.filtered_value = filtered_val;
    sp.status_flags = status_flags;

    std::memcpy(f.payload.data(), &sp, sizeof(SensorPayload));
    f.payload_length = static_cast<uint16_t>(f.payload.size());
    return f;
}

Frame Protocol::create_heartbeat_frame(uint16_t device_id, uint32_t sequence) {
    Frame f;
    f.device_id = device_id;
    f.message_type = static_cast<uint8_t>(MessageType::HEARTBEAT);
    f.sequence = sequence;
    f.timestamp = get_current_time_ms();
    f.payload_length = 0;
    return f;
}

bool Protocol::parse_sensor_payload(const std::vector<uint8_t>& payload, SensorPayload& out_sensor) {
    if (payload.size() < sizeof(SensorPayload)) {
        return false;
    }
    std::memcpy(&out_sensor, payload.data(), sizeof(SensorPayload));
    return true;
}

} // namespace industrial

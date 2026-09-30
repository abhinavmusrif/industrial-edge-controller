#include "protocol.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace industrial;

void test_crc32_standard_vector() {
    const char* test_str = "123456789";
    uint32_t crc = Protocol::calculate_crc32(reinterpret_cast<const uint8_t*>(test_str), 9);
    (void)crc;
    // Standard IEEE 802.3 CRC-32 check value is 0xCBF43926
    assert(crc == 0xCBF43926);
    std::cout << "  [PASS] test_crc32_standard_vector: matches standard IEEE 802.3 test vector 0xCBF43926\n";
}

void test_crc32_corruption_detection() {
    uint8_t payload[] = {0xAA, 0x55, 0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t original_crc = Protocol::calculate_crc32(payload, sizeof(payload));

    // Flip 1 bit in payload
    payload[3] ^= 0x01;
    uint32_t corrupted_crc = Protocol::calculate_crc32(payload, sizeof(payload));
    (void)original_crc;
    (void)corrupted_crc;

    assert(original_crc != corrupted_crc);
    std::cout << "  [PASS] test_crc32_corruption_detection: bit-flip alters checksum as expected\n";
}

void test_protocol_encode_decode_roundtrip() {
    Frame frame = Protocol::create_sensor_frame(101, 42, 1, 52.4f, 51.8f, 0x01);
    auto wire_bytes = Protocol::serialize_frame(frame, false);

    assert(wire_bytes.size() == HEADER_SIZE + sizeof(SensorPayload) + CRC_SIZE);

    Frame decoded_frame;
    DecodeResult res = Protocol::deserialize_frame(wire_bytes, decoded_frame);
    (void)res;
    assert(res == DecodeResult::SUCCESS);
    assert(decoded_frame.device_id == 101);
    assert(decoded_frame.sequence == 42);
    assert(decoded_frame.message_type == static_cast<uint8_t>(MessageType::SENSOR_DATA));

    SensorPayload sp;
    bool parsed = Protocol::parse_sensor_payload(decoded_frame.payload, sp);
    (void)parsed;
    assert(parsed);
    assert(sp.sensor_id == 1);
    assert(std::abs(sp.raw_value - 52.4f) < 0.001f);
    assert(std::abs(sp.filtered_value - 51.8f) < 0.001f);
    assert(sp.status_flags == 0x01);

    std::cout << "  [PASS] test_protocol_encode_decode_roundtrip: complete frame serialization/deserialization verified\n";
}

void test_malformed_packets_rejected() {
    // 1. Corrupted CRC
    Frame frame = Protocol::create_sensor_frame(1, 1, 1, 30.0f, 30.0f, 0);
    auto bad_bytes = Protocol::serialize_frame(frame, true); // Inject bad CRC
    Frame out;
    DecodeResult res = Protocol::deserialize_frame(bad_bytes, out);
    (void)res;
    assert(res == DecodeResult::CORRUPTED_CRC);

    // 2. Incomplete Frame
    std::vector<uint8_t> partial = {0xAA, 0x55, 0x01};
    res = Protocol::deserialize_frame(partial, out);
    assert(res == DecodeResult::NEED_MORE_DATA);

    // 3. Frame with garbage prefix followed by valid frame
    std::vector<uint8_t> noisy_stream = {0xFF, 0x00, 0x42, 0x99};
    auto valid_bytes = Protocol::serialize_frame(frame, false);
    noisy_stream.insert(noisy_stream.end(), valid_bytes.begin(), valid_bytes.end());

    res = Protocol::deserialize_frame(noisy_stream, out);
    assert(res == DecodeResult::SUCCESS);
    assert(out.sequence == 1);

    std::cout << "  [PASS] test_malformed_packets_rejected: CRC error detection and sync framing recovery verified\n";
}

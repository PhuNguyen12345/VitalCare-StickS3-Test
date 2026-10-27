#include <string.h>
#include <unity.h>

#include "protocol/BleProtocol.h"

using namespace BleProtocol;

void setUp() {}
void tearDown() {}

// Các vector này cũng được kiểm tra trong tools/ble_client.py (self-test)

void test_encode_live() {
    LiveSample sample;
    sample.epoch = 0x12345678;
    sample.temperatureX100 = 3652;
    sample.heartRate = 78;
    sample.spo2 = 98;
    sample.confidence = 95;
    sample.fingerStatus = 3;
    sample.flags = 0x04;
    sample.seq = 0x0102;

    const uint8_t expected[LIVE_SIZE] = {
        0x78, 0x56, 0x34, 0x12, 0x44, 0x0E, 0x4E, 0x00,
        0x62, 0x5F, 0x03, 0x04, 0x02, 0x01, 0x00, 0x00
    };

    uint8_t out[LIVE_SIZE];
    TEST_ASSERT_EQUAL(LIVE_SIZE, encodeLive(sample, out));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, LIVE_SIZE);
}

void test_encode_live_invalid_temperature() {
    LiveSample sample;
    uint8_t out[LIVE_SIZE];
    encodeLive(sample, out);

    TEST_ASSERT_EQUAL_HEX8(0x00, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0x80, out[5]);
}

void test_encode_status() {
    StatusPacket status;
    status.flags = StatusFlags::RTC_VALID | StatusFlags::SYNCING;
    status.battery = 87;
    status.epoch = 0x65000000;
    status.tzMinutes = 420;
    status.pendingSegments = 3;
    status.pendingBytes = 40000;
    status.fsFreeBytes = 1000000;

    const uint8_t expected[STATUS_SIZE] = {
        0x01, 0x05, 0x57, 0x00, 0x00, 0x00, 0x65, 0xA4, 0x01, 0x03,
        0x00, 0x40, 0x9C, 0x00, 0x00, 0x40, 0x42, 0x0F, 0x00
    };

    uint8_t out[STATUS_SIZE];
    TEST_ASSERT_EQUAL(STATUS_SIZE, encodeStatus(status, out));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, STATUS_SIZE);
}

void test_encode_sync_packets() {
    uint8_t out[SYNC_SEGMENT_END_SIZE];

    const uint8_t header[] = {0x07, 0x00, 0x02, 0x01, 0x00};
    TEST_ASSERT_EQUAL(SYNC_HEADER_SIZE, encodeSyncHeader(7, 0x0102, SyncType::Data, out));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(header, out, sizeof(header));

    const uint8_t end[] = {0x07, 0x00, 0x03, 0x00, 0x01, 0x10, 0x27, 0x00, 0x00, 0x26, 0x39, 0xF4, 0xCB};
    TEST_ASSERT_EQUAL(SYNC_SEGMENT_END_SIZE, encodeSegmentEnd(7, 3, 10000, 0xCBF43926, out));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(end, out, sizeof(end));
}

void test_decode_control() {
    ControlCommand command;

    const uint8_t setTime[] = {0x01, 0x78, 0x56, 0x34, 0x12, 0xD4, 0xFE};  // tz = -300
    TEST_ASSERT_TRUE(decodeControl(setTime, sizeof(setTime), command));
    TEST_ASSERT_EQUAL(ControlOp::SetTime, command.op);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, command.epoch);
    TEST_ASSERT_EQUAL_INT16(-300, command.tzMinutes);

    const uint8_t ack[] = {0x03, 0x2A, 0x01};
    TEST_ASSERT_TRUE(decodeControl(ack, sizeof(ack), command));
    TEST_ASSERT_EQUAL(ControlOp::SyncAck, command.op);
    TEST_ASSERT_EQUAL_UINT16(0x012A, command.segmentId);

    const uint8_t start[] = {0x02};
    TEST_ASSERT_TRUE(decodeControl(start, sizeof(start), command));
    TEST_ASSERT_EQUAL(ControlOp::SyncStart, command.op);
}

void test_decode_control_rejects_bad_input() {
    ControlCommand command;

    const uint8_t shortSetTime[] = {0x01, 0x00, 0x00};
    const uint8_t shortAck[] = {0x03, 0x01};
    const uint8_t unknown[] = {0x7F};

    TEST_ASSERT_FALSE(decodeControl(shortSetTime, sizeof(shortSetTime), command));
    TEST_ASSERT_FALSE(decodeControl(shortAck, sizeof(shortAck), command));
    TEST_ASSERT_FALSE(decodeControl(unknown, sizeof(unknown), command));
    TEST_ASSERT_FALSE(decodeControl(nullptr, 0, command));
}

void test_crc32_matches_zlib() {
    const char* text = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32Update(0, (const uint8_t*)text, 9));

    // Tính nối tiếp nhiều đoạn cho kết quả như tính một lần
    uint32_t crc = crc32Update(0, (const uint8_t*)text, 4);
    crc = crc32Update(crc, (const uint8_t*)text + 4, 5);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_encode_live);
    RUN_TEST(test_encode_live_invalid_temperature);
    RUN_TEST(test_encode_status);
    RUN_TEST(test_encode_sync_packets);
    RUN_TEST(test_decode_control);
    RUN_TEST(test_decode_control_rejects_bad_input);
    RUN_TEST(test_crc32_matches_zlib);
    return UNITY_END();
}

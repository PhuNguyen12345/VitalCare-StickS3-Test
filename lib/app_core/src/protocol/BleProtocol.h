#pragma once

#include <stddef.h>
#include <stdint.h>

// ======================================================
// Giao thức BLE giữa thiết bị và app điện thoại
// Mọi số nguyên đều little-endian. Chi tiết: docs/BLE_PROTOCOL.md
// ======================================================

namespace BleProtocol {

// v2: live mang nhiệt độ cơ thể ở byte 14, thêm lệnh hiệu chuẩn nhiệt độ
constexpr uint8_t VERSION = 2;

// ------------------------------------------------------
// live (READ, NOTIFY) — 16 byte, mỗi giây
// u32 epochUtc | i16 skinTemp×100 | u16 hr | u8 spo2 | u8 conf
// | u8 fingerStatus | u8 flags | u16 seq | i16 bodyTemp×100
// ------------------------------------------------------

constexpr size_t LIVE_SIZE = 16;
constexpr int16_t TEMP_INVALID = INT16_MIN;

struct LiveSample {
    uint32_t epoch = 0;
    int16_t temperatureX100 = TEMP_INVALID;
    uint16_t heartRate = 0;
    uint8_t spo2 = 0;
    uint8_t confidence = 0;
    uint8_t fingerStatus = 0;
    uint8_t flags = 0;
    uint16_t seq = 0;
    int16_t bodyTemperatureX100 = TEMP_INVALID;
};

size_t encodeLive(const LiveSample& sample, uint8_t* out);

// ------------------------------------------------------
// status (READ, NOTIFY) — 19 byte
// u8 version | u8 flags | u8 battery% | u32 epochUtc | i16 tzMinutes
// | u16 pendingSegments | u32 pendingBytes | u32 fsFreeBytes
// ------------------------------------------------------

namespace StatusFlags {

constexpr uint8_t RTC_VALID = 0x01;
constexpr uint8_t SEN_READY = 0x02;
constexpr uint8_t SYNCING = 0x04;
constexpr uint8_t LOG_OVERFLOW = 0x08;
constexpr uint8_t TEMP_CALIBRATED = 0x10;

}  // namespace StatusFlags

constexpr size_t STATUS_SIZE = 19;

struct StatusPacket {
    uint8_t flags = 0;
    uint8_t battery = 0;
    uint32_t epoch = 0;
    int16_t tzMinutes = 0;
    uint16_t pendingSegments = 0;
    uint32_t pendingBytes = 0;
    uint32_t fsFreeBytes = 0;
};

size_t encodeStatus(const StatusPacket& status, uint8_t* out);

// ------------------------------------------------------
// sync (NOTIFY)
// u16 segmentId | u16 chunkIndex | u8 type | payload
// ------------------------------------------------------

enum class SyncType : uint8_t {
    Data = 0,        // payload: byte CSV
    SegmentEnd = 1,  // payload: u32 totalBytes | u32 crc32
    SyncDone = 2     // không có payload
};

constexpr size_t SYNC_HEADER_SIZE = 5;
constexpr size_t SYNC_SEGMENT_END_SIZE = SYNC_HEADER_SIZE + 8;

size_t encodeSyncHeader(uint16_t segmentId, uint16_t chunkIndex, SyncType type, uint8_t* out);

size_t encodeSegmentEnd(
    uint16_t segmentId,
    uint16_t chunkIndex,
    uint32_t totalBytes,
    uint32_t crc,
    uint8_t* out
);

// ------------------------------------------------------
// control (WRITE) — opcode u8 + payload
// ------------------------------------------------------

enum class ControlOp : uint8_t {
    SetTime = 0x01,    // u32 epochUtc | i16 tzMinutes
    SyncStart = 0x02,
    SyncAck = 0x03,    // u16 segmentId
    SyncAbort = 0x04,
    ClearLog = 0x05,
    SetTempOffset = 0x06,  // i16 offset×100 (-32768 = về độ lệch mặc định)
    CalibrateTemp = 0x07   // i16 nhiệt độ tham chiếu×100 (nhiệt kế đo miệng)
};

struct ControlCommand {
    ControlOp op = ControlOp::SyncAbort;
    uint32_t epoch = 0;
    int16_t tzMinutes = 0;
    uint16_t segmentId = 0;
    int16_t valueX100 = 0;  // SetTempOffset / CalibrateTemp
};

bool decodeControl(const uint8_t* data, size_t length, ControlCommand& out);

// CRC32 chuẩn (giống zlib.crc32), dùng để kiểm tra segment
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length);

}  // namespace BleProtocol

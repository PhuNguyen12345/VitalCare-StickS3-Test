#include "protocol/BleProtocol.h"

namespace BleProtocol {

namespace {

template <typename T>
uint8_t* putLE(uint8_t* cursor, T value) {
    for (size_t i = 0; i < sizeof(T); i++) {
        *cursor++ = (uint8_t)((uint64_t)value >> (8 * i));
    }

    return cursor;
}

template <typename T>
T getLE(const uint8_t* data) {
    uint64_t value = 0;

    for (size_t i = 0; i < sizeof(T); i++) {
        value |= (uint64_t)data[i] << (8 * i);
    }

    return (T)value;
}

}  // namespace

size_t encodeLive(const LiveSample& sample, uint8_t* out) {
    uint8_t* cursor = out;

    cursor = putLE(cursor, sample.epoch);
    cursor = putLE(cursor, (uint16_t)sample.temperatureX100);
    cursor = putLE(cursor, sample.heartRate);
    cursor = putLE(cursor, sample.spo2);
    cursor = putLE(cursor, sample.confidence);
    cursor = putLE(cursor, sample.fingerStatus);
    cursor = putLE(cursor, sample.flags);
    cursor = putLE(cursor, sample.seq);
    cursor = putLE(cursor, (uint16_t)sample.bodyTemperatureX100);

    return cursor - out;
}

size_t encodeStatus(const StatusPacket& status, uint8_t* out) {
    uint8_t* cursor = out;

    cursor = putLE(cursor, VERSION);
    cursor = putLE(cursor, status.flags);
    cursor = putLE(cursor, status.battery);
    cursor = putLE(cursor, status.epoch);
    cursor = putLE(cursor, (uint16_t)status.tzMinutes);
    cursor = putLE(cursor, status.pendingSegments);
    cursor = putLE(cursor, status.pendingBytes);
    cursor = putLE(cursor, status.fsFreeBytes);

    return cursor - out;
}

size_t encodeSyncHeader(uint16_t segmentId, uint16_t chunkIndex, SyncType type, uint8_t* out) {
    uint8_t* cursor = out;

    cursor = putLE(cursor, segmentId);
    cursor = putLE(cursor, chunkIndex);
    cursor = putLE(cursor, (uint8_t)type);

    return cursor - out;
}

size_t encodeSegmentEnd(
    uint16_t segmentId,
    uint16_t chunkIndex,
    uint32_t totalBytes,
    uint32_t crc,
    uint8_t* out
) {
    uint8_t* cursor = out + encodeSyncHeader(segmentId, chunkIndex, SyncType::SegmentEnd, out);

    cursor = putLE(cursor, totalBytes);
    cursor = putLE(cursor, crc);

    return cursor - out;
}

bool decodeControl(const uint8_t* data, size_t length, ControlCommand& out) {
    if (data == nullptr || length == 0) {
        return false;
    }

    out = ControlCommand();

    switch ((ControlOp)data[0]) {
        case ControlOp::SetTime:
            if (length < 7) {
                return false;
            }

            out.op = ControlOp::SetTime;
            out.epoch = getLE<uint32_t>(data + 1);
            out.tzMinutes = (int16_t)getLE<uint16_t>(data + 5);
            return true;

        case ControlOp::SyncAck:
            if (length < 3) {
                return false;
            }

            out.op = ControlOp::SyncAck;
            out.segmentId = getLE<uint16_t>(data + 1);
            return true;

        case ControlOp::SetTempOffset:
        case ControlOp::CalibrateTemp:
            if (length < 3) {
                return false;
            }

            out.op = (ControlOp)data[0];
            out.valueX100 = (int16_t)getLE<uint16_t>(data + 1);
            return true;

        case ControlOp::SyncStart:
        case ControlOp::SyncAbort:
        case ControlOp::ClearLog:
            out.op = (ControlOp)data[0];
            return true;
    }

    return false;
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
    crc = ~crc;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }

    return ~crc;
}

}  // namespace BleProtocol

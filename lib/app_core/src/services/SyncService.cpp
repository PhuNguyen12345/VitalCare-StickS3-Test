#include "services/SyncService.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace BleProtocol;

namespace {

// Vị trí epoch trong gói status: bỏ qua khi so sánh để biết status có đổi không
constexpr size_t STATUS_EPOCH_OFFSET = 3;
constexpr size_t STATUS_EPOCH_SIZE = 4;

bool statusChanged(const uint8_t* previous, const uint8_t* current) {
    for (size_t i = 0; i < STATUS_SIZE; i++) {
        bool isEpoch = i >= STATUS_EPOCH_OFFSET && i < STATUS_EPOCH_OFFSET + STATUS_EPOCH_SIZE;

        if (!isEpoch && previous[i] != current[i]) {
            return true;
        }
    }

    return false;
}

}  // namespace

SyncService::SyncService(
    IConnectivity& link,
    ILogRepository& repository,
    TimeService& timeService,
    uint32_t liveIntervalMs,
    uint32_t statusIntervalMs,
    uint32_t ackTimeoutMs
)
    : link_(link),
      repository_(repository),
      timeService_(timeService),
      liveIntervalMs_(liveIntervalMs),
      statusIntervalMs_(statusIntervalMs),
      ackTimeoutMs_(ackTimeoutMs) {}

bool SyncService::begin() {
    return link_.begin();
}

void SyncService::update(uint32_t nowMs, const HealthReading& reading, const DeviceStatus& status) {
    updateConnection();

    ControlCommand command;

    while (link_.poll(command)) {
        handleCommand(command, nowMs);
    }

    if (state_ == State::WaitingAck && (int32_t)(nowMs - ackDeadlineMs_) > 0) {
        char message[48];
        snprintf(message, sizeof(message), "Sync: timeout waiting ACK for segment %u", segmentId_);
        log(message);

        state_ = State::Idle;
        statusDirty_ = true;
    }

    if (connected_) {
        sendPackets(nowMs);
        publishLive(nowMs, reading, status);
    }

    publishStatus(nowMs, status);
}

void SyncService::updateConnection() {
    bool connected = link_.isConnected();

    if (connected == connected_) {
        return;
    }

    connected_ = connected;
    statusDirty_ = true;

    if (connected_) {
        log("BLE: connected");
        return;
    }

    log("BLE: disconnected");

    // Segment chưa được ACK vẫn còn trong bộ nhớ, lần sau gửi lại
    state_ = State::Idle;
}

void SyncService::handleCommand(const ControlCommand& command, uint32_t nowMs) {
    char message[64];

    switch (command.op) {
        case ControlOp::SetTime:
            timeService_.setTime(command.epoch, command.tzMinutes);
            snprintf(
                message,
                sizeof(message),
                "BLE: set time %lu, tz %d min",
                (unsigned long)command.epoch,
                command.tzMinutes
            );
            log(message);
            break;

        case ControlOp::SyncStart:
            if (state_ == State::Idle) {
                log("Sync: start");
                repository_.rotate();
                startNextSegment();
            }
            break;

        case ControlOp::SyncAck:
            if (state_ == State::WaitingAck && command.segmentId == segmentId_) {
                bool deleted = repository_.deleteSegment(segmentId_);

                snprintf(
                    message,
                    sizeof(message),
                    "Sync: ACK segment %u, deleted %s",
                    segmentId_,
                    deleted ? "OK" : "FAILED"
                );
                log(message);

                startNextSegment();
            }
            break;

        case ControlOp::SyncAbort:
            log("Sync: aborted by phone");
            state_ = State::Idle;
            break;

        case ControlOp::ClearLog:
            log("Sync: log cleared by phone");
            repository_.clear();
            state_ = State::Idle;
            break;

        case ControlOp::SetTempOffset:
            if (temperature_ == nullptr) {
                break;
            }

            if (command.valueX100 == BleProtocol::TEMP_INVALID) {
                temperature_->resetOffset();
                log("Temp: offset reset to default");
            } else {
                bool ok = temperature_->setOffset(command.valueX100 / 100.0f);
                snprintf(message, sizeof(message), "Temp: set offset %.2f C %s", command.valueX100 / 100.0f, ok ? "OK" : "REJECTED");
                log(message);
            }
            break;

        case ControlOp::CalibrateTemp: {
            if (temperature_ == nullptr) {
                break;
            }

            // Từ chối nếu chưa cân bằng nhiệt hoặc độ lệch không hợp lý
            bool ok = temperature_->calibrate(command.valueX100 / 100.0f);

            if (ok) {
                snprintf(message, sizeof(message), "Temp: calibrated, offset %.2f C", temperature_->current().offsetC);
            } else {
                snprintf(message, sizeof(message), "Temp: calibration REJECTED (not stable or bad reference)");
            }

            log(message);
            break;
        }
    }

    (void)nowMs;
    statusDirty_ = true;
}

void SyncService::startNextSegment() {
    uint16_t id;
    uint32_t size;

    if (!repository_.oldestSegment(id, size)) {
        state_ = State::SendingDone;
        return;
    }

    char message[48];
    snprintf(message, sizeof(message), "Sync: sending segment %u (%lu bytes)", id, (unsigned long)size);
    log(message);

    segmentId_ = id;
    segmentSize_ = size;
    offset_ = 0;
    chunkIndex_ = 0;
    crc_ = 0;
    state_ = State::SendingData;
}

void SyncService::sendPackets(uint32_t nowMs) {
    for (uint8_t i = 0; i < MAX_PACKETS_PER_UPDATE; i++) {
        switch (state_) {
            case State::SendingData: {
                size_t packetSize = link_.syncPayloadSize();

                if (packetSize > PACKET_BUFFER_SIZE) {
                    packetSize = PACKET_BUFFER_SIZE;
                }

                if (packetSize <= SYNC_HEADER_SIZE) {
                    return;
                }

                uint32_t remaining = segmentSize_ - offset_;

                if (remaining == 0) {
                    state_ = State::SendingEnd;
                    break;
                }

                size_t chunk = packetSize - SYNC_HEADER_SIZE;

                if (chunk > remaining) {
                    chunk = remaining;
                }

                size_t read = repository_.readSegment(segmentId_, offset_, packet_ + SYNC_HEADER_SIZE, chunk);

                // Đọc lỗi: kết thúc segment tại vị trí hiện tại, CRC sẽ báo cho app
                if (read == 0) {
                    segmentSize_ = offset_;
                    state_ = State::SendingEnd;
                    break;
                }

                encodeSyncHeader(segmentId_, chunkIndex_, SyncType::Data, packet_);

                // Bộ đệm đầy: giữ nguyên offset, lần update sau gửi lại
                if (!link_.sendSync(packet_, SYNC_HEADER_SIZE + read)) {
                    return;
                }

                crc_ = crc32Update(crc_, packet_ + SYNC_HEADER_SIZE, read);
                offset_ += read;
                chunkIndex_++;
                break;
            }

            case State::SendingEnd: {
                size_t length = encodeSegmentEnd(segmentId_, chunkIndex_, offset_, crc_, packet_);

                if (!link_.sendSync(packet_, length)) {
                    return;
                }

                state_ = State::WaitingAck;
                ackDeadlineMs_ = nowMs + ackTimeoutMs_;
                return;
            }

            case State::SendingDone: {
                size_t length = encodeSyncHeader(0, 0, SyncType::SyncDone, packet_);

                if (!link_.sendSync(packet_, length)) {
                    return;
                }

                log("Sync: done");
                state_ = State::Idle;
                statusDirty_ = true;
                return;
            }

            case State::Idle:
            case State::WaitingAck:
                return;
        }
    }
}

void SyncService::publishLive(uint32_t nowMs, const HealthReading& reading, const DeviceStatus& status) {
    if (nowMs - lastLiveMs_ < liveIntervalMs_) {
        return;
    }

    lastLiveMs_ = nowMs;

    LiveSample sample;
    sample.epoch = timeService_.epochUtc();
    sample.seq = liveSeq_++;

    if (!timeService_.time().reliable) {
        sample.flags |= LogFlags::TIME_INVALID;
    }

    BodyTemperature body = bodyTemperature();

    if (reading.temperatureValid) {
        sample.temperatureX100 = (int16_t)lroundf(reading.temperatureC * 100.0f);
    }

    if (body.hasEstimate()) {
        sample.bodyTemperatureX100 = (int16_t)lroundf(body.bodyC * 100.0f);
    }

    sample.flags |= temperatureFlags(reading.temperatureValid, body);

    if (status.senInitialized && reading.bioValid) {
        sample.heartRate = reading.heartRate;
        sample.spo2 = (uint8_t)reading.spo2;
        sample.confidence = reading.confidence;
        sample.fingerStatus = reading.fingerStatus;
    }

    // Gửi giá trị thô, cờ cho app biết giá trị nào dùng được
    BioSampleQuality quality = evaluateBioSample(reading);

    if (!status.senInitialized || !quality.heartRateValid) {
        sample.flags |= LogFlags::BIO_INVALID;
    }

    if (!status.senInitialized || !quality.spo2Valid) {
        sample.flags |= LogFlags::SPO2_INVALID;
    }

    uint8_t data[LIVE_SIZE];
    encodeLive(sample, data);
    link_.publishLive(data, sizeof(data));
}

void SyncService::publishStatus(uint32_t nowMs, const DeviceStatus& status) {
    LogStats stats = repository_.stats();

    StatusPacket packet;
    packet.battery = status.batteryLevel;
    packet.epoch = timeService_.epochUtc();
    packet.tzMinutes = timeService_.tzMinutes();
    packet.pendingSegments = stats.segmentCount;
    packet.pendingBytes = stats.totalBytes;
    packet.fsFreeBytes = stats.freeBytes;

    if (timeService_.time().reliable) {
        packet.flags |= StatusFlags::RTC_VALID;
    }

    if (status.senInitialized) {
        packet.flags |= StatusFlags::SEN_READY;
    }

    if (syncing()) {
        packet.flags |= StatusFlags::SYNCING;
    }

    if (stats.overflow) {
        packet.flags |= StatusFlags::LOG_OVERFLOW;
    }

    if (bodyTemperature().calibrated) {
        packet.flags |= StatusFlags::TEMP_CALIBRATED;
    }

    uint8_t data[STATUS_SIZE];
    encodeStatus(packet, data);

    bool due = nowMs - lastStatusMs_ >= statusIntervalMs_;

    if (!statusDirty_ && !due && !statusChanged(lastStatus_, data)) {
        return;
    }

    if (link_.publishStatus(data, sizeof(data))) {
        memcpy(lastStatus_, data, sizeof(data));
        lastStatusMs_ = nowMs;
        statusDirty_ = false;
    }
}

void SyncService::log(const char* message) const {
    if (logger_ != nullptr) {
        logger_(message);
    }
}

BodyTemperature SyncService::bodyTemperature() const {
    return temperature_ != nullptr ? temperature_->current() : BodyTemperature();
}

#pragma once

#include <stdint.h>

#include "domain/DeviceStatus.h"
#include "domain/HealthReading.h"
#include "ports/IConnectivity.h"
#include "ports/ILogRepository.h"
#include "protocol/BleProtocol.h"
#include "services/BodyTemperatureEstimator.h"
#include "services/TimeService.h"

// Gửi dữ liệu realtime, trạng thái, và đồng bộ batch các segment log về điện thoại
class SyncService {
public:
    using LogFn = void (*)(const char* message);

    SyncService(
        IConnectivity& link,
        ILogRepository& repository,
        TimeService& timeService,
        uint32_t liveIntervalMs,
        uint32_t statusIntervalMs,
        uint32_t ackTimeoutMs
    );

    void setLogger(LogFn logger) { logger_ = logger; }

    // Nhiệt độ cơ thể cho gói live và lệnh hiệu chuẩn (có thể không gắn)
    void setTemperatureEstimator(BodyTemperatureEstimator* estimator) { temperature_ = estimator; }

    bool begin();
    void update(uint32_t nowMs, const HealthReading& reading, const DeviceStatus& status);

    bool connected() const { return connected_; }
    bool syncing() const { return state_ != State::Idle; }
    LogStats logStats() const { return repository_.stats(); }

private:
    enum class State : uint8_t {
        Idle,
        SendingData,
        SendingEnd,
        WaitingAck,
        SendingDone
    };

    // Số gói sync tối đa mỗi lần update, để không chặn vòng loop
    static constexpr uint8_t MAX_PACKETS_PER_UPDATE = 4;
    static constexpr size_t PACKET_BUFFER_SIZE = 256;

    void updateConnection();
    void handleCommand(const BleProtocol::ControlCommand& command, uint32_t nowMs);
    void startNextSegment();
    void sendPackets(uint32_t nowMs);
    void publishLive(uint32_t nowMs, const HealthReading& reading, const DeviceStatus& status);
    void publishStatus(uint32_t nowMs, const DeviceStatus& status);
    void log(const char* message) const;
    BodyTemperature bodyTemperature() const;

    IConnectivity& link_;
    ILogRepository& repository_;
    TimeService& timeService_;
    BodyTemperatureEstimator* temperature_ = nullptr;

    uint32_t liveIntervalMs_;
    uint32_t statusIntervalMs_;
    uint32_t ackTimeoutMs_;
    LogFn logger_ = nullptr;

    bool connected_ = false;

    uint32_t lastLiveMs_ = 0;
    uint16_t liveSeq_ = 0;

    uint32_t lastStatusMs_ = 0;
    bool statusDirty_ = true;
    uint8_t lastStatus_[BleProtocol::STATUS_SIZE] = {0};

    State state_ = State::Idle;
    uint16_t segmentId_ = 0;
    uint32_t segmentSize_ = 0;
    uint32_t offset_ = 0;
    uint16_t chunkIndex_ = 0;
    uint32_t crc_ = 0;
    uint32_t ackDeadlineMs_ = 0;

    uint8_t packet_[PACKET_BUFFER_SIZE];
};

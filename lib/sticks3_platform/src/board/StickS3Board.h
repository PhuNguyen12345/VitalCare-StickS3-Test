#pragma once

#include <M5Unified.h>

#include "domain/DeviceStatus.h"
#include "ports/IInputSource.h"

// Khởi tạo phần cứng M5StickS3 và đọc nút thật
class StickS3Board : public IInputSource {
public:
    void begin(uint32_t serialBaud, uint8_t rotation, uint8_t brightness, uint32_t holdThresholdMs);

    // Gọi mỗi vòng loop: cập nhật trạng thái nút
    void update();

    bool poll(AppEvent& out) override;

    M5GFX& display() { return M5.Display; }

    // 0..100 (%), DeviceStatus::BATTERY_UNKNOWN nếu không đọc được.
    // Quy đổi tuyến tính từ điện áp nên chỉ là ước lượng
    uint8_t batteryLevel();

    bool isCharging();

    // Lý do của lần khởi động gần nhất (esp_reset_reason), để chẩn đoán reset bất thường
    static const char* resetReason();

private:
    void push(AppEvent event);

    static constexpr uint8_t QUEUE_SIZE = 4;

    AppEvent queue_[QUEUE_SIZE];
    uint8_t queueCount_ = 0;
};

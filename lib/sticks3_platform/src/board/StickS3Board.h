#pragma once

#include <M5Unified.h>

#include "ports/IInputSource.h"

// Khởi tạo phần cứng M5StickS3 và đọc nút thật
class StickS3Board : public IInputSource {
public:
    void begin(uint32_t serialBaud, uint8_t rotation, uint8_t brightness, uint32_t holdThresholdMs);

    // Gọi mỗi vòng loop: cập nhật trạng thái nút
    void update();

    bool poll(AppEvent& out) override;

    M5GFX& display() { return M5.Display; }

    // 0..100 (%), 0 nếu không đọc được
    uint8_t batteryLevel();

private:
    void push(AppEvent event);

    static constexpr uint8_t QUEUE_SIZE = 4;

    AppEvent queue_[QUEUE_SIZE];
    uint8_t queueCount_ = 0;
};

#pragma once

#include <Arduino.h>
#include <Wire.h>

// Chuyển bus Wire giữa các cặp chân (Grove RTC <-> Hat2)
class I2cBusManager {
public:
    I2cBusManager(TwoWire& wire, uint32_t frequency, uint16_t timeoutMs);

    // Không khởi tạo lại nếu đã ở đúng bus
    bool select(uint8_t sda, uint8_t scl);

    TwoWire& wire() { return wire_; }

    // ======================================================
    // Tự kiểm tra bus (chẩn đoán đứt dây / mất nguồn cảm biến)
    // ======================================================

    struct BusCheck {
        bool sdaPulledUp = false;  // Có điện trở kéo lên (board cảm biến đang có nguồn)
        bool sclPulledUp = false;
        uint8_t found = 0;         // Số địa chỉ phản hồi trong danh sách
    };

    // Đo mức nghỉ của SDA/SCL rồi dò các địa chỉ, in kết quả ra Serial
    BusCheck check(
        const char* name,
        uint8_t sda,
        uint8_t scl,
        const uint8_t* addresses,
        size_t count
    );

private:
    static constexpr uint8_t NO_PIN = 255;

    TwoWire& wire_;
    uint32_t frequency_;
    uint16_t timeoutMs_;

    uint8_t currentSDA_ = NO_PIN;
    uint8_t currentSCL_ = NO_PIN;
};

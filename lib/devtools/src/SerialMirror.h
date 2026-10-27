#pragma once

#include "BuildConfig.h"

#if VC_ENABLE_DEVTOOLS

#include <Arduino.h>

#include "UiController.h"

// Gửi canvas qua Serial: chụp một lần, tự chụp khi đổi trang,
// hoặc mirror liên tục (chỉ gửi dải dòng thay đổi).
//
// Gói v2 (số nguyên little-endian):
//   MAGIC(6) | version u8=2 | flags u8 | width u16 | height u16 | y u16 | rows u16
//   | nameLen u8 | name | pixels RGB565 (width*rows*2, byte cao trước) | crc32 u32
class SerialMirror : public FrameListener {
public:
    static constexpr uint8_t FLAG_CAPTURE = 0x01;

    explicit SerialMirror(Print& out);

    void setMirrorEnabled(bool enabled);
    bool mirrorEnabled() const { return mirrorEnabled_; }

    void requestCapture() { captureRequested_ = true; }
    bool toggleAutoCapture();

    void onFrame(const uint8_t* pixels, uint16_t width, uint16_t height, const char* name) override;

private:
    static constexpr uint16_t MAX_ROWS = 240;
    static constexpr uint8_t MAX_NAME_LENGTH = 32;
    static constexpr uint32_t WRITE_TIMEOUT_MS = 3000;

    bool sendRows(
        const uint8_t* pixels,
        uint16_t width,
        uint16_t height,
        uint16_t y,
        uint16_t rows,
        const char* name,
        uint8_t flags
    );

    bool writeAll(const uint8_t* data, size_t length, uint32_t deadline);

    Print& out_;

    bool mirrorEnabled_ = false;
    bool captureRequested_ = false;
    bool autoCapture_ = false;

    // CRC từng dòng của frame gần nhất, để tìm dải dòng thay đổi
    uint32_t rowCrc_[MAX_ROWS];
    bool cacheValid_ = false;

    char lastName_[MAX_NAME_LENGTH + 1] = {0};
};

#endif

#pragma once

#include <M5GFX.h>

#include "ports/IDisplay.h"
#include "screens/BootScreen.h"
#include "screens/HomeScreen.h"
#include "screens/MeasuringScreen.h"

// Nhận mỗi frame sau khi vẽ xong (vd: mirror qua Serial)
class FrameListener {
public:
    virtual ~FrameListener() = default;

    // pixels: buffer RGB565 của canvas, byte cao trước
    virtual void onFrame(const uint8_t* pixels, uint16_t width, uint16_t height, const char* name) = 0;
};

class UiController : public IDisplay {
public:
    explicit UiController(M5GFX& display);

    bool begin(int16_t width, int16_t height);

    void showBoot();
    void render(const AppViewModel& model) override;

    // Dải màu kiểm tra thứ tự byte RGB565 phía Python
    void showTestPattern();

    void setFrameListener(FrameListener* listener) { frameListener_ = listener; }

private:
    Screen& screenFor(ScreenPage page);
    void present(const char* name);

    M5Canvas canvas_;
    FrameListener* frameListener_ = nullptr;

    BootScreen bootScreen_;
    HomeScreen homeScreen_;
    MeasuringScreen temperatureScreen_{MeasuringScreen::Metric::Temperature};
    MeasuringScreen heartRateScreen_{MeasuringScreen::Metric::HeartRate};
    MeasuringScreen spo2Screen_{MeasuringScreen::Metric::SpO2};
};

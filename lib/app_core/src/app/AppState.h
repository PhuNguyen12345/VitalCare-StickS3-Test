#pragma once

#include <stdint.h>

enum class ScreenPage : uint8_t {
    Clock,
    Temperature,
    HeartRate,
    SpO2
};

// Tên trang, dùng cho tên file ảnh chụp và dòng @DATA
inline const char* screenPageName(ScreenPage page) {
    switch (page) {
        case ScreenPage::Clock:
            return "clock";

        case ScreenPage::Temperature:
            return "temperature";

        case ScreenPage::HeartRate:
            return "heart_rate";

        case ScreenPage::SpO2:
            return "spo2";
    }

    return "unknown";
}

struct AppState {
    ScreenPage page = ScreenPage::Clock;
    bool redrawRequested = true;
};

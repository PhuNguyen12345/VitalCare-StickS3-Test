#pragma once

#include <stdint.h>

enum class AppEvent : uint8_t {
    BtnAClick,
    BtnAHold,
    BtnBClick,
    BtnBHold
};

inline const char* appEventName(AppEvent event) {
    switch (event) {
        case AppEvent::BtnAClick:
            return "BtnAClick";

        case AppEvent::BtnAHold:
            return "BtnAHold";

        case AppEvent::BtnBClick:
            return "BtnBClick";

        case AppEvent::BtnBHold:
            return "BtnBHold";
    }

    return "Unknown";
}

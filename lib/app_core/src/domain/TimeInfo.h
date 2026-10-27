#pragma once

#include <stdint.h>

struct TimeInfo {
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    bool valid = false;
};

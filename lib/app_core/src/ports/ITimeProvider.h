#pragma once

#include "domain/TimeInfo.h"

class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;

    virtual bool begin() = 0;
    virtual bool read(TimeInfo& out) = 0;
};

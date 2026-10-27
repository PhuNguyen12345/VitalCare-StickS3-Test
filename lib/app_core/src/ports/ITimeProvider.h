#pragma once

#include "domain/TimeInfo.h"

class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;

    virtual bool begin() = 0;
    virtual bool read(TimeInfo& out) = 0;

    // Ghi ngày giờ (UTC) xuống đồng hồ
    virtual bool write(const TimeInfo& time) = 0;
};

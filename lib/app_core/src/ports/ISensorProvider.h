#pragma once

#include "domain/DeviceStatus.h"
#include "domain/HealthReading.h"

class ISensorProvider {
public:
    virtual ~ISensorProvider() = default;

    virtual bool begin() = 0;

    // Chỉ ghi vào phần dữ liệu của cảm biến này
    virtual bool read(HealthReading& out) = 0;

    // Ghi trạng thái của cảm biến này vào DeviceStatus
    virtual void reportStatus(DeviceStatus& status) const = 0;
};

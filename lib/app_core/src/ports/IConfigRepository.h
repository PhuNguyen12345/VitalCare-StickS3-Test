#pragma once

#include <stdint.h>

// Cấu hình lưu bền (NVS)
class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    virtual bool begin() = 0;

    // Múi giờ, tính bằng phút so với UTC (Việt Nam: +420)
    virtual int16_t tzMinutes(int16_t defaultValue) = 0;
    virtual void setTzMinutes(int16_t minutes) = 0;

    // Độ lệch nhiệt độ cơ thể (°C × 100); false nếu chưa hiệu chuẩn
    virtual bool tempOffsetX100(int16_t& out) = 0;
    virtual void setTempOffsetX100(int16_t value) = 0;
    virtual void clearTempOffset() = 0;
};

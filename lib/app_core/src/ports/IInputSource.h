#pragma once

#include "app/AppEvent.h"

// Nguồn sự kiện đầu vào: nút thật, lệnh Serial từ simulator...
class IInputSource {
public:
    virtual ~IInputSource() = default;

    // Trả về true và ghi vào out nếu còn sự kiện chưa xử lý
    virtual bool poll(AppEvent& out) = 0;
};

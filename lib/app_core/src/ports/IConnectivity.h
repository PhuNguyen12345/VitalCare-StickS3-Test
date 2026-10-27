#pragma once

#include <stddef.h>
#include <stdint.h>

#include "protocol/BleProtocol.h"

// Kênh kết nối với điện thoại (BLE). Chỉ truyền byte đã mã hóa,
// việc mã hóa/giải mã nằm ở protocol/BleProtocol.
class IConnectivity {
public:
    virtual ~IConnectivity() = default;

    virtual bool begin() = 0;
    virtual bool isConnected() const = 0;

    // Số byte tối đa của một gói sync (MTU - 3)
    virtual size_t syncPayloadSize() const = 0;

    virtual bool publishLive(const uint8_t* data, size_t length) = 0;
    virtual bool publishStatus(const uint8_t* data, size_t length) = 0;

    // false nếu bộ đệm đầy: gửi lại sau
    virtual bool sendSync(const uint8_t* data, size_t length) = 0;

    // Lệnh từ điện thoại (đã giải mã)
    virtual bool poll(BleProtocol::ControlCommand& out) = 0;
};

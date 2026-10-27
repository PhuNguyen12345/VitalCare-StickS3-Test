#pragma once

// ======================================================
// Cấu hình BLE — phải khớp với docs/BLE_PROTOCOL.md và app điện thoại
// ======================================================

namespace BleConfig {

// Tên quảng bá: "VitalCare-XXXX" (4 ký tự hex cuối địa chỉ MAC)
constexpr const char* NAME_PREFIX = "VitalCare-";

constexpr const char* SERVICE_UUID = "06b40001-765e-4d79-a126-f95ef3affd61";
constexpr const char* LIVE_UUID = "06b40002-765e-4d79-a126-f95ef3affd61";
constexpr const char* CONTROL_UUID = "06b40003-765e-4d79-a126-f95ef3affd61";
constexpr const char* SYNC_UUID = "06b40004-765e-4d79-a126-f95ef3affd61";
constexpr const char* STATUS_UUID = "06b40005-765e-4d79-a126-f95ef3affd61";

// MTU đề nghị; Android cần gọi requestMtu(185)
constexpr uint16_t PREFERRED_MTU = 185;

}  // namespace BleConfig

# Giao thức BLE — VitalCare StickS3

Tài liệu cho nhóm phát triển app điện thoại (Flutter). Mô tả cách app kết nối với thiết bị VitalCare, nhận số liệu thời gian thực, chỉnh giờ và đồng bộ dữ liệu đã lưu.

- Phiên bản giao thức: **1** (byte đầu của gói `status`)
- Mã nguồn tham chiếu: [`lib/app_core/src/protocol/BleProtocol.h`](../lib/app_core/src/protocol/BleProtocol.h), [`include/BleConfig.h`](../include/BleConfig.h)
- Client mẫu chạy trên máy tính: [`tools/ble_client.py`](../tools/ble_client.py)

---

## 1. Tổng quan

| Mục | Giá trị |
|---|---|
| Tên quảng bá | `VitalCare-XXXX` (XXXX = 4 ký tự hex cuối địa chỉ MAC) |
| Bảo mật | Không pairing (giai đoạn test) |
| Thứ tự byte | **Little-endian** cho mọi số nguyên |
| MTU đề nghị | 185. Android phải gọi `requestMtu(185)`, iOS tự thương lượng |
| Số kết nối | Mỗi lần nên chỉ có 1 điện thoại kết nối |

Thiết bị đo liên tục và:
- gửi **số liệu realtime** mỗi 1 giây khi có điện thoại kết nối;
- mỗi **10 giây** lưu một bản ghi trung bình vào bộ nhớ trong (khoảng 6 ngày dữ liệu), kể cả khi không có kết nối;
- khi app yêu cầu, gửi các bản ghi đã lưu theo từng **segment**. App xác nhận (ACK) từng segment, thiết bị chỉ xóa segment sau khi nhận được ACK.

---

## 2. Service và characteristic

Service: **`06b40001-765e-4d79-a126-f95ef3affd61`**

| Tên | UUID | Thuộc tính | Hướng | Nội dung |
|---|---|---|---|---|
| `live` | `06b40002-765e-4d79-a126-f95ef3affd61` | READ, NOTIFY | Thiết bị → app | Số liệu realtime, 16 byte, mỗi giây |
| `control` | `06b40003-765e-4d79-a126-f95ef3affd61` | WRITE, WRITE NO RESPONSE | App → thiết bị | Lệnh (chỉnh giờ, đồng bộ...) |
| `sync` | `06b40004-765e-4d79-a126-f95ef3affd61` | NOTIFY | Thiết bị → app | Dữ liệu đồng bộ batch |
| `status` | `06b40005-765e-4d79-a126-f95ef3affd61` | READ, NOTIFY | Thiết bị → app | Trạng thái thiết bị, 19 byte |

---

## 3. Định dạng gói

### 3.1 `live` — 16 byte

| Offset | Kiểu | Trường | Ghi chú |
|---|---|---|---|
| 0 | u32 | `epochUtc` | Giây từ 1970-01-01 UTC. `0` nếu chưa đọc được giờ |
| 4 | i16 | `temperatureX100` | Nhiệt độ da × 100 (36.52 °C → `3652`). **`-32768` (0x8000) = không hợp lệ** |
| 6 | u16 | `heartRate` | Nhịp tim (bpm) |
| 8 | u8 | `spo2` | SpO2 (%) |
| 9 | u8 | `confidence` | Độ tin cậy của thuật toán (%) |
| 10 | u8 | `fingerStatus` | 0 = không có vật, 1 = chưa sẵn sàng, 2 = có vật, **3 = có ngón tay** |
| 11 | u8 | `flags` | Xem [mục 3.5](#35-bit-flags-của-live-và-bản-ghi) |
| 12 | u16 | `seq` | Tăng 1 mỗi gói, dùng để phát hiện gói bị mất |
| 14 | u16 | — | Dự phòng, bỏ qua |

Chỉ hiển thị HR/SpO2 khi bit `BIO_INVALID` **tắt**.

### 3.2 `status` — 19 byte

| Offset | Kiểu | Trường | Ghi chú |
|---|---|---|---|
| 0 | u8 | `protocolVersion` | Hiện là `1` |
| 1 | u8 | `flags` | `0x01` RTC_VALID, `0x02` SEN_READY, `0x04` SYNCING, `0x08` LOG_OVERFLOW |
| 2 | u8 | `battery` | Pin (%) |
| 3 | u32 | `epochUtc` | Giờ hiện tại của thiết bị |
| 7 | i16 | `tzMinutes` | Múi giờ đang dùng (phút, Việt Nam = `420`) |
| 9 | u16 | `pendingSegments` | Số segment chưa đồng bộ (kể cả segment đang ghi) |
| 11 | u32 | `pendingBytes` | Tổng số byte chưa đồng bộ |
| 15 | u32 | `fsFreeBytes` | Bộ nhớ trống |

Thiết bị gửi notify khi trạng thái thay đổi, và ít nhất 5 giây một lần.

- `RTC_VALID` tắt: giờ thiết bị chưa đúng. App nên gửi `SET_TIME`.
- `LOG_OVERFLOW` bật: bộ nhớ từng bị đầy và dữ liệu cũ nhất đã bị xóa. Cờ tự tắt khi đã đồng bộ hết.

### 3.3 `control` — app ghi xuống

Byte đầu là opcode, phần còn lại là payload.

| Opcode | Lệnh | Payload | Tổng | Ý nghĩa |
|---|---|---|---|---|
| `0x01` | `SET_TIME` | u32 `epochUtc`, i16 `tzMinutes` | 7 byte | Chỉnh đồng hồ thiết bị (RTC lưu UTC) và múi giờ hiển thị |
| `0x02` | `SYNC_START` | — | 1 byte | Bắt đầu gửi dữ liệu đã lưu |
| `0x03` | `SYNC_ACK` | u16 `segmentId` | 3 byte | Xác nhận đã nhận đúng segment; thiết bị xóa segment đó và gửi segment kế tiếp |
| `0x04` | `SYNC_ABORT` | — | 1 byte | Dừng đồng bộ (segment chưa ACK được giữ lại) |
| `0x05` | `CLEAR_LOG` | — | 1 byte | **Xóa toàn bộ dữ liệu đã lưu.** Chỉ dùng khi debug |

Ví dụ `SET_TIME` với epoch `0x12345678` và múi giờ −300 phút: `01 78 56 34 12 D4 FE`.

### 3.4 `sync` — thiết bị gửi lên

Mỗi gói gồm header 5 byte và payload:

| Offset | Kiểu | Trường |
|---|---|---|
| 0 | u16 | `segmentId` |
| 2 | u16 | `chunkIndex` |
| 4 | u8 | `type` |
| 5… | | payload |

| `type` | Tên | Payload |
|---|---|---|
| `0` | `DATA` | Một đoạn nội dung file CSV của segment (tối đa MTU − 8 byte) |
| `1` | `SEGMENT_END` | u32 `totalBytes`, u32 `crc32` (CRC-32 chuẩn, giống zlib) của toàn bộ segment |
| `2` | `SYNC_DONE` | Không có. Đã gửi hết dữ liệu |

### 3.5 Bit `flags` của `live` và bản ghi

| Bit | Tên | Ý nghĩa |
|---|---|---|
| `0x01` | `TIME_INVALID` | Giờ chưa được chỉnh. Với bản ghi lưu, `ts` là số giây kể từ khi thiết bị khởi động |
| `0x02` | `TEMP_INVALID` | Không có nhiệt độ hợp lệ |
| `0x04` | `BIO_INVALID` | Không có ngón tay hoặc chưa có mẫu HR/SpO2 hợp lệ |
| `0x08` | `LOOSE` | Dự phòng: cảnh báo đeo lỏng |
| `0x10` | `ALERT` | Dự phòng: cảnh báo vượt ngưỡng |

### 3.6 Nội dung segment (CSV)

Ghép các payload `DATA` của một segment sẽ được một file CSV:

```
#v1,ts,t100,hr,spo2,conf,flags
1761550000,3652,78,98,95,0
1761550010,3655,80,97,92,0
1761550020,3649,0,0,0,4
```

| Cột | Ý nghĩa |
|---|---|
| `ts` | Epoch UTC (giây) ở cuối cửa sổ 10 giây |
| `t100` | Nhiệt độ trung bình × 100 (`0` nếu `TEMP_INVALID`) |
| `hr`, `spo2`, `conf` | Trung bình các mẫu có ngón tay (`0` nếu `BIO_INVALID`) |
| `flags` | Như [mục 3.5](#35-bit-flags-của-live-và-bản-ghi) |

Dòng bắt đầu bằng `#` là header: `v1` là phiên bản định dạng. Mỗi segment khoảng 16 KB, tương đương khoảng 600 bản ghi hay 1,7 giờ đo.

---

## 4. Luồng hoạt động

### 4.1 Kết nối

```
App                                   Thiết bị
 │  scan theo SERVICE_UUID               │
 │  connect                              │
 │  requestMtu(185)  (Android)           │
 │  discoverServices                     │
 │  bật notify: live, status, sync       │
 │  read status ───────────────────────► │
 │ ◄─────────────────── status (19 byte) │
 │  nếu RTC_VALID tắt hoặc lệch giờ:     │
 │  write control: SET_TIME ───────────► │
 │ ◄──────────── live (mỗi giây)         │
```

Nên gửi `SET_TIME` ở mỗi lần kết nối. Lệnh này rẻ, và giữ đồng hồ thiết bị luôn đúng.

### 4.2 Đồng bộ batch

```
App                                              Thiết bị
 │  write control: SYNC_START ─────────────────► │  đóng segment đang ghi
 │ ◄──────────── sync DATA (seg 12, chunk 0)     │
 │ ◄──────────── sync DATA (seg 12, chunk 1)     │
 │ ◄──────────── ...                             │
 │ ◄──────────── sync SEGMENT_END (seg 12)       │
 │  kiểm tra totalBytes + crc32                  │
 │  lưu vào DB của app                           │
 │  write control: SYNC_ACK(12) ───────────────► │  xóa segment 12
 │ ◄──────────── sync DATA (seg 13, chunk 0)     │
 │ ◄──────────── ...                             │
 │ ◄──────────── sync SYNC_DONE                  │
```

Quy tắc:
- **Chỉ ACK sau khi đã lưu dữ liệu an toàn** (vào database của app). Sau khi nhận ACK, thiết bị xóa segment vĩnh viễn.
- Sai `totalBytes` hoặc `crc32`: **không ACK**. Bỏ dữ liệu đã nhận của segment đó, gửi `SYNC_ABORT`, rồi `SYNC_START` lại.
- Thiết bị chờ ACK tối đa **10 giây**. Quá thời gian, thiết bị dừng đồng bộ và giữ nguyên segment.
- Mất kết nối giữa chừng: segment chưa ACK vẫn còn trên thiết bị, lần đồng bộ sau sẽ gửi lại **từ đầu segment**.
- Có thể nhận trùng một segment đã lưu, ví dụ app lưu xong nhưng mất kết nối trước khi ACK. App nên khử trùng theo `ts`.
- Trong lúc đồng bộ, gói `live` vẫn tiếp tục được gửi.

---

## 5. Code mẫu Flutter (`flutter_blue_plus`)

Đoạn code dưới đây chỉ minh họa luồng chính. API của `flutter_blue_plus` thay đổi giữa các phiên bản, nên hãy đối chiếu với tài liệu của bản nhóm đang dùng.

```dart
import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:archive/archive.dart' show getCrc32;
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

final serviceUuid = Guid('06b40001-765e-4d79-a126-f95ef3affd61');
final liveUuid = Guid('06b40002-765e-4d79-a126-f95ef3affd61');
final controlUuid = Guid('06b40003-765e-4d79-a126-f95ef3affd61');
final syncUuid = Guid('06b40004-765e-4d79-a126-f95ef3affd61');
final statusUuid = Guid('06b40005-765e-4d79-a126-f95ef3affd61');

class LiveSample {
  final int epochUtc;
  final double? temperature; // null nếu không hợp lệ
  final int heartRate, spo2, confidence, fingerStatus, flags, seq;

  LiveSample.fromBytes(List<int> value)
      : this._(ByteData.sublistView(Uint8List.fromList(value)));

  LiveSample._(ByteData b)
      : epochUtc = b.getUint32(0, Endian.little),
        temperature = b.getInt16(4, Endian.little) == -32768
            ? null
            : b.getInt16(4, Endian.little) / 100.0,
        heartRate = b.getUint16(6, Endian.little),
        spo2 = b.getUint8(8),
        confidence = b.getUint8(9),
        fingerStatus = b.getUint8(10),
        flags = b.getUint8(11),
        seq = b.getUint16(12, Endian.little);

  bool get bioValid => flags & 0x04 == 0;
}

Uint8List setTimeCommand(DateTime now) {
  final b = ByteData(7)
    ..setUint8(0, 0x01)
    ..setUint32(1, now.toUtc().millisecondsSinceEpoch ~/ 1000, Endian.little)
    ..setInt16(5, now.timeZoneOffset.inMinutes, Endian.little);
  return b.buffer.asUint8List();
}

Uint8List syncAckCommand(int segmentId) {
  final b = ByteData(3)
    ..setUint8(0, 0x03)
    ..setUint16(1, segmentId, Endian.little);
  return b.buffer.asUint8List();
}

Future<void> connectAndSync(BluetoothDevice device) async {
  await device.connect();
  if (Platform.isAndroid) {
    await device.requestMtu(185);
  }

  final service = (await device.discoverServices())
      .firstWhere((s) => s.uuid == serviceUuid);
  BluetoothCharacteristic char(Guid uuid) =>
      service.characteristics.firstWhere((c) => c.uuid == uuid);

  final live = char(liveUuid);
  final control = char(controlUuid);
  final sync = char(syncUuid);

  // Realtime
  await live.setNotifyValue(true);
  live.onValueReceived.listen((value) {
    final sample = LiveSample.fromBytes(value);
    // cập nhật UI...
  });

  // Chỉnh giờ mỗi lần kết nối
  await control.write(setTimeCommand(DateTime.now()));

  // Đồng bộ batch
  final buffer = BytesBuilder();
  final done = Completer<void>();

  await sync.setNotifyValue(true);
  final subscription = sync.onValueReceived.listen((value) async {
    final b = ByteData.sublistView(Uint8List.fromList(value));
    final segmentId = b.getUint16(0, Endian.little);
    final type = b.getUint8(4);

    if (type == 0) {
      buffer.add(value.sublist(5));
    } else if (type == 1) {
      final totalBytes = b.getUint32(5, Endian.little);
      final crc = b.getUint32(9, Endian.little);
      final data = buffer.takeBytes();

      if (data.length == totalBytes && getCrc32(data) == crc) {
        // 1. Parse CSV và lưu vào database của app
        // 2. Sau khi lưu xong mới ACK
        await control.write(syncAckCommand(segmentId));
      } else {
        await control.write([0x04]); // SYNC_ABORT, thử lại sau
      }
    } else if (type == 2) {
      done.complete();
    }
  });

  await control.write([0x02]); // SYNC_START
  await done.future.timeout(const Duration(minutes: 5));
  await subscription.cancel();
}
```

Lưu ý khi viết app:
- Notify của `sync` đến liên tục. Hãy xử lý tuần tự: không gửi ACK của segment N+1 trước khi lưu xong segment N.
- Parse CSV: bỏ dòng bắt đầu bằng `#`, tách theo dấu `,`. Mỗi dòng có đúng 6 cột.
- `ts` là UTC. Khi hiển thị, đổi sang giờ địa phương ở phía app.

---

## 6. Kiểm thử không cần app

Dùng [`tools/ble_client.py`](../tools/ble_client.py) trên máy tính (cần `pip install bleak`):

```bash
python3 tools/ble_client.py scan       # thấy VitalCare-XXXX
python3 tools/ble_client.py settime    # chỉnh giờ theo máy tính
python3 tools/ble_client.py live       # in số liệu realtime
python3 tools/ble_client.py sync       # đồng bộ, lưu CSV vào ble_logs/
python3 tools/ble_client.py selftest   # kiểm tra bộ giải mã với vector của firmware
```

Hoặc dùng app **nRF Connect** trên điện thoại: kết nối thiết bị, bật notify `live`, xem 16 byte gửi lên mỗi giây.

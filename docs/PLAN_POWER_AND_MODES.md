# Plan: Tiết kiệm pin và chế độ hoạt động của thiết bị IoMT

> Trạng thái: **Bản nháp để nhóm review** — 2026-10-28
> Phạm vi: firmware M5StickS3 (VitalCare). Các mục cần nhóm chốt được đánh dấu ❓.

## 1. Bối cảnh

- Thiết bị đã có 2 kênh BLE: `live` (realtime, read-only, 1 gói/giây) và `sync` (đồng bộ batch các file CSV trong LittleFS). Chi tiết: [BLE_PROTOCOL.md](BLE_PROTOCOL.md).
- **Dữ liệu chính thức là CSV batch** (đẩy từ LittleFS). `live` chỉ để hiển thị tức thời, không lưu vào hồ sơ.
- Nghiệp vụ: **đo định kỳ**, không cần đo liên tục.
- Mục tiêu pin: **3–4 ngày** cho một lần sạc, pin 250 mAh.

## 2. Ngân sách điện năng

| Mục tiêu | Dòng trung bình tối đa |
|---|---|
| 3 ngày (72 giờ) | ≈ 3,5 mA |
| 4 ngày (96 giờ) | ≈ 2,6 mA |

Hiện tại màn hình, CPU (240 MHz) và cảm biến chạy liên tục, nên thiết bị khó trụ quá một ngày. Muốn đạt mục tiêu, thiết bị phải **ngủ sâu phần lớn thời gian** và chỉ thức dậy để đo.

Ước lượng sơ bộ (thức khoảng 45 giây mỗi lần, ≈ 60 mA khi thức):

| Chu kỳ đo | Dòng trung bình khi đo | Ghi chú |
|---|---|---|
| 15 phút | ≈ 3 mA | Rất sát mục tiêu |
| 30 phút | ≈ 1,5 mA | Có khả năng đạt 3–4 ngày |

Con số trên **chưa tính dòng rò của cả bo mạch khi ngủ**. Phải đo thực tế (bước 1 của lộ trình).

## 3. Các trạng thái hoạt động đề xuất

```
            ┌──────────── hết giờ / tắt live ─────────────┐
            ▼                                             │
      ┌──────────┐   hẹn giờ    ┌────────────┐            │
      │  NGỦ SÂU │ ───────────► │ ĐO ĐỊNH KỲ │ ──► ghi CSV ──► mở BLE ngắn ──┐
      └──────────┘              └────────────┘                (sync batch)│
         ▲    │ bấm nút                │ vượt ngưỡng                      │
         │    ▼                        ▼                                  │
         │  ┌──────────┐         ┌──────────┐                             │
         │  │ LIVE     │         │ CẢNH BÁO │ đo lại xác nhận, BLE lâu hơn │
         │  └──────────┘         └──────────┘                             │
         └────────────────────────────────────────────────────────────────┘
```

| Trạng thái | Hành vi |
|---|---|
| **Ngủ sâu** | Màn hình, cảm biến, BLE đều tắt. Hẹn giờ ESP32 (hoặc ngắt RTC) đánh thức |
| **Đo định kỳ** | Đo tối đa ~60 giây, dừng sớm khi đủ mẫu hợp lệ. Ghi **1 bản ghi / lần đo**. Mở quảng bá BLE 10–20 giây để điện thoại sync batch, rồi ngủ lại |
| **Live** | Bệnh nhân bấm nút, thiết bị đo liên tục, app đăng ký `live`. Tự tắt khi app hủy đăng ký hoặc sau ~5 phút |
| **Cảnh báo** | Vượt ngưỡng thì đo lại để xác nhận. Nếu đúng: gắn cờ `ALERT`, báo tại chỗ, quảng bá BLE dày hơn để gửi sớm. Không phát hiện ngón tay + nhiệt độ giảm: cờ `LOOSE` |

**Hệ quả cần chấp nhận:** khi thiết bị ngủ sâu, điện thoại không kết nối được. Muốn xem live phải bấm nút trên thiết bị, hoặc đợi tới cửa sổ quảng bá kế tiếp.

## 4. Ánh xạ vai trò

| Vai trò | Dữ liệu | Nguồn |
|---|---|---|
| Patient | Live (khi ở cạnh thiết bị) + lịch sử | BLE trực tiếp + backend |
| Caregiver | Lần đo gần nhất + lịch sử + cảnh báo | Backend (trễ tối đa 1 chu kỳ đo) |
| Doctor | Lịch sử + cảnh báo của bệnh nhân được phân công | Backend |
| Healthcare Staff | Thiết bị đã cấp phát, pin, cảnh báo | Backend + gói `status` |
| Clinic Admin | Toàn bộ | Backend |

Ghi chú:
- **Caregiver ở xa không có live đúng nghĩa.** BLE chỉ nối thiết bị với điện thoại bệnh nhân; điện thoại tải dữ liệu lên backend ngay sau mỗi lần sync.
- **Backend là nơi quyết định cảnh báo** (theo ngưỡng riêng của từng bệnh nhân). Cảnh báo trên thiết bị chỉ để phản ứng tại chỗ và ưu tiên gửi sớm.
- **Cấp phát thiết bị** cần mã định danh ổn định: thêm số serial và phiên bản firmware (gói `status` hoặc Device Information Service).
- **Ngưỡng cảnh báo do bác sĩ đặt**, gửi xuống thiết bị qua lệnh mới `SET_THRESHOLDS`.

## 5. Lộ trình

| # | Việc | Phụ thuộc |
|---|---|---|
| 0 | **Chuẩn hóa số đo**, bắt đầu từ nhiệt độ (xem [mục 6](#6-bước-0--chuẩn-hóa-nhiệt-độ-cơ-thể)) | — |
| 1 | Đo dòng tiêu thụ ở 4 cấu hình: hiện tại / tắt màn hình / ngủ sâu / ngủ sâu + tắt cảm biến | — |
| 2 | Màn hình tự tắt + đo định kỳ có ngủ sâu + cửa sổ BLE sau mỗi lần đo | 1 |
| 3 | Phiên live theo nút bấm | 2 |
| 4 | Cảnh báo: đo lại xác nhận, cờ `ALERT`/`LOOSE`, lệnh `SET_THRESHOLDS` | 2 |
| 5 | Định danh thiết bị cho nghiệp vụ cấp phát | — |

## 6. Bước 0 — Chuẩn hóa nhiệt độ cơ thể

### Hiện trạng
- MAX30205 đo **da cổ tay, mặt mu tay**. Vị trí này xa lõi cơ thể và chịu ảnh hưởng mạnh của nhiệt độ phòng. Đo thử trên thiết bị: 32–34 °C.
- **Chưa có nhiệt kế y tế** làm mốc, nên chưa thể hiệu chuẩn hay chứng minh độ chính xác.
- **Mỗi thiết bị chỉ có một MAX30205** (đã hàn cố định), nên chưa thể bù nhiệt độ môi trường hay dùng phương pháp dòng nhiệt hai cảm biến.

Không có phép cộng hằng số nào biến nhiệt độ da cổ tay thành nhiệt độ cơ thể chính xác. Mục tiêu thực tế là một **nhiệt độ cơ thể ước tính**, có cờ chất lượng đi kèm, và giữ lại số đo thô để cải tiến sau.

### Giai đoạn A — làm ngay (chỉ firmware, không cần thiết bị mới)

| # | Việc | Mục đích |
|---|---|---|
| A1 | **Thí nghiệm tự sinh nhiệt:** để thiết bị trên bàn (không đeo), so sánh số đo khi màn hình/BLE bật và khi tắt | Biết nhiệt tỏa ra từ chính thiết bị có làm sai số đo không |
| A2 | **Thí nghiệm thời gian cân bằng:** đeo lên tay, ghi số đo mỗi giây trong 10–15 phút | Biết cần chờ bao lâu, chọn ngưỡng "đã ổn định" |
| A3 | **Phát hiện cân bằng nhiệt:** chỉ chấp nhận số đo khi thay đổi dưới khoảng 0,05 °C/phút (ngưỡng chốt theo A2). Chưa ổn định thì bật cờ `TEMP_UNSTABLE` | Không báo số khi cảm biến chưa cân bằng |
| A4 | **Phát hiện không đeo:** nhiệt độ quá thấp so với da hoặc giảm nhanh thì bật cờ | Tránh ghi nhiệt độ của không khí; dùng lại cho cảnh báo `LOOSE` |
| A5 | **Nhiệt độ cơ thể ước tính** = nhiệt độ da + độ lệch. Độ lệch mặc định cấu hình được qua BLE, có cờ `TEMP_UNCALIBRATED` | Có con số để hiển thị, nhưng ghi rõ là chưa hiệu chuẩn |

Về dữ liệu: CSV **giữ nhiệt độ da thô**, thêm nhiệt độ cơ thể ước tính và các cờ (định dạng `#v3`). Nhờ vậy backend có thể tính lại khi cải tiến thuật toán.

### Giai đoạn B — khi có nhiệt kế y tế
- **B1. Hiệu chuẩn theo từng bệnh nhân khi check-in:** Healthcare Staff đo bằng nhiệt kế, nhập vào app, app gửi xuống thiết bị. Thiết bị tính độ lệch riêng của người đó sau khi cân bằng nhiệt, rồi tắt cờ `TEMP_UNCALIBRATED`.
- **B2. Kiểm chứng cho báo cáo:** đo song song với nhiệt kế trên vài người, nhiều lần. Tính sai số trung bình và độ lệch chuẩn (phân tích Bland–Altman).

### Giai đoạn C — tùy chọn, cần thay đổi phần cứng
- Thêm một MAX30205 thứ hai quay ra ngoài để bù nhiệt độ môi trường, hoặc làm phương pháp dòng nhiệt hai cảm biến.
- Hoặc đổi vị trí đo sang nách/ngực.

## 7. Câu hỏi cần nhóm chốt

- [ ] ❓ **Chu kỳ đo** mong muốn: 15 / 30 / 60 phút?
- [ ] ❓ Bệnh nhân **phải bấm nút trên thiết bị mới xem được live** — có chấp nhận được không?
- [ ] ❓ **Độ trễ cảnh báo** chấp nhận được: vài phút, hay gần như tức thì?
- [ ] ❓ **M5StickS3 là phần cứng cuối cùng** của đồ án, hay chỉ là bản thử nghiệm?
- [ ] ❓ Có dùng **âm thanh (loa/buzzer)** để báo cảnh báo tại chỗ không?
- [ ] ❓ Ai đặt **ngưỡng cảnh báo** (bác sĩ cho từng bệnh nhân, hay ngưỡng chung của phòng khám)?
- [ ] ❓ Backend/App: điện thoại bệnh nhân có tải dữ liệu lên **ngay sau mỗi lần sync** không (để Caregiver xem gần realtime)?
- [ ] ❓ Nhóm có mua **nhiệt kế y tế** (đo tai hoặc trán) để hiệu chuẩn và kiểm chứng không? (cần cho giai đoạn B và cho báo cáo)
- [ ] ❓ App hiển thị nhiệt độ thế nào khi **chưa hiệu chuẩn**: hiện số kèm nhãn "ước tính", hay ẩn đi?

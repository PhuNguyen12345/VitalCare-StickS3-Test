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

Hiện tại màn hình, CPU (240 MHz) và cảm biến chạy liên tục, nên thiết bị khó trụ quá một ngày.

**Đo thật 2026-11-14:**
- Cấu hình: màn hình luôn bật, không có BLE live.
- Rút USB lúc 17:21 với pin 95%. Bản ghi CSV cuối cùng lúc 18:39.
- Kết quả: chạy được **khoảng 1 giờ 20 phút**.
- Dòng trung bình **≈ 180 mA**, công suất khoảng 0,65 W. Con số này cao gấp khoảng 60 lần ngân sách.
- Số 95% chỉ là ước lượng tuyến tính theo điện áp của M5Unified, nên kết quả chỉ mang tính tham khảo.

Muốn đạt mục tiêu, thiết bị phải **ngủ sâu phần lớn thời gian** và chỉ thức dậy để đo.

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
- **Đã có nhiệt kế điện tử đo miệng** làm mốc (từ 2026-11-03). Số đo ghi trong [temperature_calibration.csv](temperature_calibration.csv).
- **Mỗi thiết bị chỉ có một MAX30205** (đã hàn cố định), nên chưa thể bù nhiệt độ môi trường hay dùng phương pháp dòng nhiệt hai cảm biến.

Không có phép cộng hằng số nào biến nhiệt độ da cổ tay thành nhiệt độ cơ thể chính xác. Mục tiêu thực tế là một **nhiệt độ cơ thể ước tính**, có cờ chất lượng đi kèm, và giữ lại số đo thô để cải tiến sau.

### Điểm tham chiếu

"Nhiệt độ cơ thể" của thiết bị được định nghĩa là **tương đương nhiệt độ đo ở miệng**. Theo Geneva và cộng sự (2019), *Normal Body Temperature: A Systematic Review*, *Open Forum Infectious Diseases* 6(4):ofz032 (36 nghiên cứu, 7.636 người trưởng thành khỏe mạnh):

| Vị trí | Trung bình | Khoảng bình thường (TB ± 2SD) |
|---|---|---|
| **Miệng** | **36,57 °C** | **35,73–37,41 °C** |
| Tai | 36,64 °C | 35,76–37,52 °C |
| Nách | 35,97 °C | 35,01–36,93 °C |
| Hậu môn | 37,04 °C | 36,32–37,76 °C |

- Người từ 60 tuổi trở lên thấp hơn khoảng 0,2–0,3 °C.
- Ngưỡng sốt thường dùng cho người lớn là ≥ 38 °C. Hướng dẫn IMCI của WHO (cho trẻ em) dùng mốc đo nách ≥ 37,5 °C.
- Nhiệt độ da cổ tay có nhịp ngày đêm **ngược** với nhiệt độ lõi: ban đêm da cổ tay tăng, nhiệt độ lõi giảm (Polar Research Center, 2023). Vì vậy một độ lệch cố định không thể đúng suốt cả ngày.

### Nhật ký hiệu chuẩn

[temperature_calibration.csv](temperature_calibration.csv): mỗi lần đo một dòng.

| Cột | Ý nghĩa |
|---|---|
| `date`, `time` | Thời điểm đo (giờ Việt Nam) |
| `person` | Người được đo (ghi tên viết tắt) |
| `ambient_c` | Nhiệt độ phòng |
| `oral_c` | Nhiệt độ miệng, đo bằng nhiệt kế |
| `wrist_c` | Nhiệt độ cổ tay |
| `wrist_source` | `max30205` nếu do cảm biến của thiết bị đo (bắt buộc khi hiệu chuẩn), `thermometer` nếu do nhiệt kế đo |
| `wear_minutes` | Thiết bị đã đeo bao lâu trước khi đo |
| `offset_c` | `oral_c − wrist_c` |
| `thermometer` | Loại / model nhiệt kế |
| `notes` | Ghi chú (vừa ăn uống, vừa vận động…) |

**Khi đo:** không ăn uống 15–30 phút trước, ngậm kín miệng, đặt đầu đo sâu dưới lưỡi, chờ nhiệt kế báo xong. Chỉ đo khi thiết bị đã đeo đủ lâu để cân bằng nhiệt.

**Kết quả tạm thời** (1 người, 2026-11-03):
- **Đã cân bằng, đo dưới lưỡi** (22:05, 22:08): độ lệch **khoảng 2,2 °C** (2,23 và 2,15). Khi đã cân bằng, số cổ tay đứng yên ở 34,15–34,20 °C, gõ phím không làm thay đổi đáng kể. **Đây là độ lệch mặc định tạm thời cho A5.**
- **Chưa cân bằng** (21:57–21:59): độ lệch 2,7 → 2,2 °C, vì số cổ tay còn đang tăng (33,1 → 34,0 °C trong 2 phút). Không dùng để tính.
- **Thời gian cân bằng** (bắt đầu đeo 21:48): số cổ tay tăng nhanh ở phút 9–11, rồi **đứng yên từ khoảng phút 17** (dao động dưới 0,05 °C). Đề xuất tạm thời: **chờ ít nhất 15 phút sau khi đeo**. Trong suốt thời gian đo, thiết bị đeo liên tục, không tháo ra hay khởi động lại.
- **Đặt nhiệt kế gần họng** (22:10): cao hơn khoảng 0,6–0,7 °C so với đo dưới lưỡi, dù số cổ tay không đổi. **Cách đặt nhiệt kế ảnh hưởng lớn hơn sai số cảm biến**, nên quy trình đo phải thống nhất. Không dùng để tính.

**Các lần đo tiếp theo:** đeo thiết bị ít nhất 15 phút, ngồi yên. Các lần đo cách nhau 5–10 phút, để nhiệt kế nguội giữa các lần. Cần khoảng 10–20 lần đo, trên vài người, ở nhiều thời điểm và nhiệt độ phòng khác nhau, trước khi chốt.

### Giai đoạn A — làm ngay (chỉ firmware, không cần thiết bị mới)

| # | Việc | Mục đích |
|---|---|---|
| A1 | ✅ **Thí nghiệm tự sinh nhiệt** (14/11): để thiết bị trên bàn (không đeo), so sánh số đo khi bật và khi tắt BLE live. Kết quả ở mục [Kết quả A1, A2](#kết-quả-a1-a2-2026-11-14) | Biết nhiệt tỏa ra từ chính thiết bị có làm sai số đo không |
| A2 | ◐ **Thí nghiệm thời gian cân bằng** (14/11): đeo lên tay, ghi số đo mỗi giây. Đã có một đường cong, nhưng không bắt đầu từ trạng thái nguội | Biết cần chờ bao lâu, chọn ngưỡng "đã ổn định" |
| A3 | ✅ **Phát hiện cân bằng nhiệt:** độ dốc (hồi quy tuyến tính) trong 3 phút gần nhất ≤ 0,05 °C/phút. Chưa ổn định thì bật cờ `TEMP_UNSTABLE`. **Cập nhật 12/11:** có số tạm thời sau 1 phút đeo, dự đoán nhiệt độ da lúc cân bằng (hằng số thời gian 6 phút, rút từ 2 lần đeo thật) | Có số nhanh như đồng hồ thông minh, vẫn đánh dấu là số tạm thời |
| A4 | ✅ **Phát hiện không đeo:** nhiệt độ da < 30 °C thì bật cờ `LOOSE` và bắt đầu lại cửa sổ cân bằng | Tránh ghi nhiệt độ của không khí |
| A5 | ✅ **Nhiệt độ cơ thể ước tính** = trung bình da 1 phút + độ lệch (mặc định 2,2 °C). Hiệu chuẩn qua BLE (`CALIBRATE_TEMP`, `SET_TEMP_OFFSET`), lưu NVS. Chưa hiệu chuẩn thì bật cờ `TEMP_UNCALIBRATED` | Có con số để hiển thị, ghi rõ là ước tính |

Về dữ liệu: CSV `#v3` **giữ nhiệt độ da thô** (`t100`), thêm nhiệt độ cơ thể ước tính (`tb100`) và các cờ. Nhờ vậy backend có thể tính lại khi cải tiến thuật toán. Các ngưỡng nằm trong `TemperatureThresholds` ([BodyTemperature.h](../lib/app_core/src/domain/BodyTemperature.h)). Chi tiết giao thức: mục 3.7 của [BLE_PROTOCOL.md](BLE_PROTOCOL.md).

### Kết quả A1, A2 (2026-11-14)

Dữ liệu, biểu đồ và script nằm trong [experiments/](experiments/):
- `2026-11-14_warmup/`: vẽ bằng `tools/plot_temperature.py`.
- `2026-11-14_selfheat/`: vẽ bằng `tools/plot_selfheat.py`.

**A1, tự sinh nhiệt** (thiết bị nằm trên bàn, lấy trung bình 3 phút cuối mỗi giai đoạn):

| Giai đoạn | Nhiệt độ cảm biến |
|---|---|
| Đang sạc | 30,74 °C |
| Rút USB, không live, sau 10 phút | 29,92 °C (vẫn đang nguội) |
| Bật BLE live khoảng 15 phút | 31,07 °C (đã ổn định sau khoảng 10 phút) |

- **Bật live làm cảm biến nóng thêm ít nhất 1,15 °C.**
- Ngay cả khi không live và không sạc, cảm biến trên bàn vẫn đo 28,8–30 °C. Phòng lúc đó khoảng 20 °C (ước chừng, không đo). Như vậy **thiết bị tự sưởi cảm biến cao hơn nhiệt độ phòng nhiều độ**: khoảng 0,65 W tỏa ra trong một khối nhỏ xếp chồng ngay trên MAX30205.

**A2, đeo trên tay, có BLE live suốt buổi** (đeo lúc 16:30, tháo 16:38–16:40, ghi từ 16:40):
- Da ổn định ở **35,99 °C**, hằng số thời gian khớp được τ ≈ 8,3 phút. Trạng thái Stable sau khoảng 19 phút.
- Số tạm thời dự đoán nhất quán với số Stable cuối cùng. Cách cộng thẳng thì chậm khoảng 2 °C trong giai đoạn đầu.
- Số tạm thời dao động khoảng ±0,4 °C, nên cần làm mượt.
- **Độ lệch miệng − da chỉ còn 0,1–0,3 °C**, thay vì 2,2 °C. Nhiệt kế đo miệng được 35,8–36,2 °C, nên thiết bị báo cao hơn khoảng 2 °C.
- Ngày 03/11, da chỉ ổn định ở khoảng 34,1 °C. Chênh lệch khoảng 1,8 °C so với hôm nay phần lớn do BLE live làm nóng thêm (A1). Phần còn lại có thể do điều kiện khác nhau.

**Kết luận:**
- **Độ lệch da → cơ thể phụ thuộc vào chế độ chạy của thiết bị** (màn hình, BLE, sạc). Độ lệch 2,2 °C chỉ đúng với chế độ dùng lúc đo nó.
- **Chưa hiệu chuẩn lại lúc này.** Hiệu chuẩn sau khi đã có ngủ sâu và đo định kỳ (bước 2 của lộ trình), đúng chế độ thiết bị chạy thật. Khi đó cần đo lại τ và độ lệch.
- Khi demo trên tay mà đang bật live, số đo cao hơn khoảng 1 °C.
- Ngưỡng không đeo "da < 30 °C" **không đủ khi phòng ấm**: máy nằm trên bàn vẫn có lúc trên 30 °C, nên bị coi là đang đeo. Cần kết hợp thêm tín hiệu tiếp xúc da từ SEN (bộ cảm biến nhịp tim, SpO2).

Còn thiếu: đường cong cân bằng đầy đủ từ lúc thiết bị nguội (A2), đo không bật live.

### Giai đoạn B — hiệu chuẩn bằng nhiệt kế (đã có nhiệt kế, đang thu số liệu)
- **B1. Hiệu chuẩn theo từng bệnh nhân khi check-in:** Healthcare Staff đo bằng nhiệt kế, nhập vào app, app gửi xuống thiết bị. Thiết bị tính độ lệch riêng của người đó sau khi cân bằng nhiệt, rồi tắt cờ `TEMP_UNCALIBRATED`.
- **B2. Kiểm chứng cho báo cáo:** đo song song với nhiệt kế trên vài người, nhiều lần. Tính sai số trung bình và độ lệch chuẩn (phân tích Bland–Altman).

### Giai đoạn C — tùy chọn, cần thay đổi phần cứng
- **Cách nhiệt** giữa StickS3/RTC và bo cảm biến (miếng xốp dày hơn băng dính hai mặt), để cảm biến ít bị thiết bị sưởi nóng (xem A1).
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
- [x] ~~Nhóm có mua nhiệt kế y tế không?~~ Đã có nhiệt kế điện tử đo miệng (2026-11-03)
- [ ] ❓ App hiển thị nhiệt độ thế nào khi **chưa hiệu chuẩn**: hiện số kèm nhãn "ước tính", hay ẩn đi?

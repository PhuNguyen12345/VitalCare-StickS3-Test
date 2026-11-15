# VitalCare StickS3 — firmware thiết bị IoMT (project test)

Ngữ cảnh cho session mới. Người dùng trao đổi bằng **tiếng Việt**, nên trả lời bằng tiếng Việt.

## Dự án

- Đồ án **SEP490** gồm 3 tầng:
  - **Web:** trang quản trị phòng khám.
  - **App mobile (Flutter):** theo dõi bệnh nhân và chỉ số sinh hiệu.
  - **IoMT:** thiết bị đeo tay đo trực tiếp. Repo này thuộc tầng này.
- Đây là **project test** của firmware, **chưa phải project chính**. Ưu tiên code dễ kiểm thử, tách module.
- **Vai trò liên quan đến IoMT:**
  - Patient: xem dữ liệu live của bản thân.
  - Caregiver: người thân, xem từ xa qua backend.
  - Doctor: xem bệnh nhân được phân công, xử lý cảnh báo.
  - Healthcare Staff: check-in bệnh nhân, cấp phát và quản lý thiết bị, chuyển cảnh báo cho bác sĩ.
  - Clinic Admin: xem toàn bộ.

## Phần cứng

- **Bộ điều khiển:** M5Stack **M5StickS3** (ESP32-S3, 8MB flash, LCD 240×135, pin 250 mAh), dùng PlatformIO, Arduino core 2.0.17, thư viện M5Unified/M5GFX.
- Có 2 thiết bị: StickS3 là sản phẩm chính đeo tay; thiết bị màn hình 2.8 inch là prototype. **Mỗi thiết bị có một bộ cảm biến hàn cố định.**
- **Đấu dây** (khai báo trong [include/BoardPins.h](include/BoardPins.h), sơ đồ và ảnh trong [docs/HARDWARE.md](docs/HARDWARE.md)):

  | Linh kiện | Kết nối |
  |---|---|
  | MAX30205 (nhiệt độ, `0x48`) | Bus Hat2: **SDA G7, SCL G6** |
  | SEN-15219 / MAX32664 (HR, SpO2, `0x55`) | Bus Hat2: SDA G7, SCL G6; **RST G1, MFIO G8** |
  | RTC HYM8563 (`0x51`) | Cổng **Grove: SDA G9, SCL G10** |

  - `I2cBusManager` chuyển `Wire` qua lại giữa hai bus trước mỗi lần đọc.
  - MAX30205 đo **da cổ tay, mặt mu tay**.
- LCD từng bị vỡ, đã thay màn hình mới (11/2026). **Simulator** trên máy tính phản chiếu màn hình và có nút ảo, vẫn hữu ích để xem log và chụp màn hình.

## Kiến trúc

Chi tiết trong [STRUCTURE.md](STRUCTURE.md).

| Lib | Vai trò |
|---|---|
| `lib/app_core` | Logic thuần, **không include Arduino**, test được trên máy tính. Chứa `domain/`, `ports/` (các interface `I*`), `services/`, `protocol/BleProtocol` |
| `lib/sticks3_platform` | Driver phần cứng: board, I2C, cảm biến, RTC, LittleFS, NVS, BLE (NimBLE-Arduino 2.x) |
| `lib/ui` | `UiController`, các screen vẽ bằng `M5Canvas`. Hiện có 4 trang: Clock, Temperature, HeartRate, SpO2 |
| `lib/devtools` | Mirror màn hình và nút ảo qua Serial, dòng `@DATA`. Bật/tắt bằng `VC_ENABLE_DEVTOOLS` |

[src/main.cpp](src/main.cpp) chỉ là nơi lắp ráp các thành phần.

## Lệnh thường dùng

```bash
~/.platformio/penv/bin/pio run                  # build firmware
~/.platformio/penv/bin/pio run -t upload        # nạp
~/.platformio/penv/bin/pio test -e native       # test app_core trên máy tính (Unity)

# Công cụ Python. Trên macOS 14 dùng Python 3.14 bản python.org,
# vì Python Homebrew không có Tkinter và cài python-tk bằng brew bị lỗi
/Library/Frameworks/Python.framework/Versions/3.14/bin/python3 -m venv .venv
.venv/bin/pip install -r tools/requirements.txt

.venv/bin/python tools/vc_simulator.py          # mirror màn hình + nút A/B ảo (tắt Serial Monitor trước)
.venv/bin/python tools/canvas_capture.py        # Serial monitor + chụp PNG
.venv/bin/python tools/ble_client.py scan|status|live|settime|sync|selftest
```

## Quyết định đã chốt

- **BLE:** 4 characteristic `live`, `control`, `sync`, `status`, không pairing (giai đoạn test). Giao thức: [docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md).
  - `live`: gói nhị phân 16 byte mỗi giây, **chỉ để hiển thị, không lưu vào hồ sơ**. Mất gói thì bỏ qua; `seq` chỉ dùng để phát hiện mất gói.
  - **CSV đồng bộ batch từ LittleFS là dữ liệu chính thức.** Thiết bị chỉ xóa segment sau khi nhận ACK; app chỉ được ACK sau khi đã lưu dữ liệu an toàn.
- **Lưu trữ:**
  - Mỗi 10 giây ghi một bản ghi trung bình.
  - Mỗi segment là một file `/log/NNNNN.csv`, tối đa khoảng 16KB.
  - Segment đóng khi app gửi `SYNC_START`, khi đầy, hoặc **mỗi lần khởi động lại**.
  - Định dạng CSV hiện tại là `#v3`: `ts,t100,tb100,hr,spo2,conf,flags` (`t100` = da thô, `tb100` = cơ thể ước tính). Giao thức BLE phiên bản 2.
- **Thời gian:**
  - RTC lưu **UTC**; múi giờ lưu trong NVS (mặc định +420). App chỉnh giờ bằng lệnh `SET_TIME`.
  - `ts` là epoch, **không phải id**. Khóa đề xuất cho backend là `(mã thiết bị, ts)`.
- **Đánh giá mẫu SEN:** một hàm duy nhất `evaluateBioSample()` trong [HealthReading.h](lib/app_core/src/domain/HealthReading.h), dùng chung cho màn hình, gói `live` và CSV.
  - Nhịp tim hợp lệ khi có ngón tay (`fingerStatus == 3`) **và** `conf ≥ 50`.
  - SpO2 hợp lệ khi nhịp tim hợp lệ **và** `0 < spo2 ≤ 100`.
  - `conf` là độ tin cậy của **nhịp tim**.
  - Cờ: `BIO_INVALID` (4), `SPO2_INVALID` (32).
- **Nhiệt độ cơ thể ước tính:** `BodyTemperatureEstimator` + `TemperatureThresholds` trong [BodyTemperature.h](lib/app_core/src/domain/BodyTemperature.h). Dùng chung cho màn hình, gói `live` (byte 14) và CSV (`tb100`).
  - Cơ thể = nhiệt độ da + độ lệch (mặc định 2,2 °C). **Có số sau 1 phút đeo** (yêu cầu của giáo viên: lên số nhanh như đồng hồ thông minh): khi chưa cân bằng, nhiệt độ da được dự đoán = trung bình 1 phút + 6 phút × độ dốc (bù 0–2,5 °C), cờ `TEMP_UNSTABLE`. **Đã cân bằng** (độ dốc 3 phút ≤ 0,05 °C/phút) thì dùng trung bình 1 phút; chỉ lúc này mới cho hiệu chuẩn. Da < 30 °C thì coi là không đeo.
  - Cờ: `LOOSE` (8, không đeo), `TEMP_UNSTABLE` (64), `TEMP_UNCALIBRATED` (128).
  - Hiệu chuẩn bằng lệnh BLE `CALIBRATE_TEMP` (nhiệt độ miệng tham chiếu) hoặc `SET_TEMP_OFFSET`; độ lệch lưu NVS. Công cụ: `ble_client.py calibrate --ref 36.4`.
- **Nghiệp vụ:**
  - **Đo định kỳ**, không cần đo liên tục.
  - **Pin mục tiêu 3–4 ngày.**
  - Nhiệt độ cần là **nhiệt độ cơ thể**. Cảm biến hiện đo nhiệt độ da, cần chuẩn hóa.

## Lưu ý dễ vấp

- **Mở cổng Serial là reset chip** (`rst:0x15 USB_UART_CHIP_RESET`): mỗi lần simulator, Serial Monitor hay esptool kết nối, thiết bị khởi động lại, mở segment mới và bộ ước tính nhiệt độ chạy lại từ đầu (1 phút mới có số). Dòng `Boot: reset reason = ...` cho biết lý do. Panic in ra UART0 nên không thấy qua USB; core dump lưu ở partition `coredump` (`0x7F0000`), đọc bằng `esptool read_flash`.
- **Giữ `CORE_DEBUG_LEVEL=3`.** Mức 5 làm log hệ thống chen vào và hỏng frame mirror.
- **Lần `settime` đầu tiên** sau khi chuyển sang lưu UTC tạo bước nhảy −7 giờ trong dữ liệu cũ, vì RTC trước đó giữ giờ Việt Nam.
- **Nhiệt độ MAX30205 là nhiệt độ da** (khoảng 32–34 °C ở cổ tay). Không phải thân nhiệt.
- **Thiết bị tự sưởi nóng cảm biến** (thí nghiệm A1, 14/11):
  - Nằm trên bàn, phòng khoảng 20 °C, cảm biến vẫn đo 29–30 °C.
  - Bật BLE live: nóng thêm ≥ 1,15 °C. Đang sạc cũng làm nóng thêm.
  - Hệ quả: độ lệch da → cơ thể phụ thuộc vào chế độ chạy. Khi đo hiệu chuẩn phải ghi rõ chế độ chạy lúc đo.
  - Da < 30 °C (ngưỡng không đeo) không đủ khi phòng ấm.
- **Pin đo thật: khoảng 1 giờ 20 phút** khi màn hình luôn bật, không ngủ (≈ 180 mA). Mục tiêu 3–4 ngày, nên bắt buộc phải có ngủ sâu.
- **Phần trăm pin chỉ là ước lượng:** M5Unified quy đổi tuyến tính từ điện áp, nên số sụt theo tải và nhảy lên khi cắm sạc. `0xFF` (`DeviceStatus::BATTERY_UNKNOWN`) nghĩa là không đọc được pin.
- **SpO2 từ SEN-15219 chưa hiệu chuẩn** cho vỏ thiết bị, chỉ dùng để tham khảo.
- **Khởi tạo SEN mất khoảng 5 giây** (có `delay(4000)`). Sẽ là vấn đề khi làm ngủ sâu, vì mỗi lần thức là khởi động lại.
- **`ble_logs/`, `captures/`, `.venv/`, `__pycache__/`** đều đã có trong `.gitignore`.

## Trạng thái và việc tiếp theo

Plan chi tiết và câu hỏi còn mở: [docs/PLAN_POWER_AND_MODES.md](docs/PLAN_POWER_AND_MODES.md).

- **Đã xong:** refactor theo STRUCTURE.md, simulator, BLE realtime + batch (đã chạy thật trên thiết bị), quy tắc đánh giá mẫu, nhiệt độ cơ thể ước tính A3–A5 (CSV `#v3`, giao thức v2; đã đo thử trên thiết bị, kết quả gần với nhiệt kế và được chấp nhận), header pin + Bluetooth trên mọi trang (**chưa chạy thử trên thiết bị**).
- **Đang làm — bước 0, chuẩn hóa nhiệt độ cơ thể:**
  - **Đã làm A1 và một phần A2 (14/11):**
    - Dữ liệu và biểu đồ nằm trong `docs/experiments/`; công cụ vẽ là `tools/plot_temperature.py` và `tools/plot_selfheat.py`.
    - Kết quả ghi ở mục "Kết quả A1, A2" của PLAN.
  - **Đã quyết định chưa hiệu chuẩn lại.** Sẽ hiệu chuẩn (độ lệch và τ) sau khi có ngủ sâu và đo định kỳ, đúng chế độ thiết bị chạy thật.
  - Còn lại:
    - Làm mượt số tạm thời (dao động khoảng ±0,4 °C).
    - Đo đường cong A2 từ lúc thiết bị nguội, không bật live.
  - Đã có nhiệt kế điện tử đo miệng. Mốc tham chiếu: **nhiệt độ miệng, trung bình 36,57 °C** (Geneva 2019). Số đo hiệu chuẩn ghi trong [docs/temperature_calibration.csv](docs/temperature_calibration.csv); độ lệch tạm thời (miệng − cổ tay) khoảng **2,2 °C**, từ 2 lần đo khi cảm biến **đã cân bằng nhiệt**. Chỉ dùng các dòng ghi chú `da can bang`.
- **Sau đó:** đo dòng tiêu thụ, rồi ngủ sâu + đo định kỳ + cửa sổ BLE; phiên live bằng nút bấm; cảnh báo; định danh thiết bị.
- **Đang chờ nhóm quyết định:** chu kỳ đo, nút SOS, chu kỳ đo thích ứng, phát hiện té ngã bằng IMU.

## Quy ước làm việc

- **Code:**
  - Thụt lề 4 dấu cách, comment tiếng Việt.
  - Đầu mỗi phần có banner `// ====`.
  - Lời gọi hàm nhiều tham số thì xuống dòng mỗi tham số.
- **Kiểm tra:** sau mỗi thay đổi phải `pio run` thành công và `pio test -e native` pass. Logic mới trong `app_core` phải có test native.
- **Đổi giao thức BLE hoặc định dạng CSV:** cập nhật cùng lúc `BleProtocol` + test, `tools/ble_client.py` (`selftest`) và `docs/BLE_PROTOCOL.md`.
- **Commit:** người dùng tự commit, theo dạng `feat: ...`. Chỉ commit khi được yêu cầu.

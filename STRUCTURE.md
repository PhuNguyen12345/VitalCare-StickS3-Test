m5sticks3-health-device/
├── platformio.ini
├── .gitignore
├── README.md
│
├── include/
│   ├── AppConfig.h
│   ├── BoardPins.h
│   └── BuildConfig.h
│
├── src/
│   └── main.cpp
│
├── lib/
│   ├── app_core/
│   │   └── src/
│   │       ├── app/
│   │       │   ├── App.h
│   │       │   ├── App.cpp
│   │       │   ├── AppState.h
│   │       │   ├── AppEvent.h
│   │       │   └── AppViewModel.h
│   │       │
│   │       ├── domain/
│   │       │   ├── HealthReading.h
│   │       │   ├── MeasurementLog.h
│   │       │   ├── DeviceStatus.h
│   │       │   └── TimeInfo.h
│   │       │
│   │       ├── ports/
│   │       │   ├── ISensorProvider.h
│   │       │   ├── ITimeProvider.h
│   │       │   ├── IInputSource.h          (*)
│   │       │   ├── ILogRepository.h
│   │       │   ├── IConnectivity.h
│   │       │   ├── IAlertService.h
│   │       │   └── IDisplay.h
│   │       │
│   │       └── services/
│   │           ├── MeasurementService.h
│   │           ├── MeasurementService.cpp
│   │           ├── TimeService.h
│   │           ├── TimeService.cpp
│   │           ├── LogService.h
│   │           ├── LogService.cpp
│   │           ├── SyncService.h
│   │           └── SyncService.cpp
│   │
│   ├── sticks3_platform/
│   │   └── src/
│   │       ├── board/
│   │       │   ├── StickS3Board.h
│   │       │   ├── StickS3Board.cpp
│   │       │   ├── I2cBusManager.h         (*)
│   │       │   └── I2cBusManager.cpp       (*)
│   │       │
│   │       ├── sensors/
│   │       │   ├── MockSensorProvider.h
│   │       │   ├── MockSensorProvider.cpp
│   │       │   ├── SenSensorProvider.h
│   │       │   ├── SenSensorProvider.cpp
│   │       │   ├── Max30205Provider.h
│   │       │   └── Max30205Provider.cpp
│   │       │
│   │       ├── time/
│   │       │   ├── RtcTimeProvider.h
│   │       │   ├── NtpTimeProvider.h
│   │       │   └── UptimeProvider.h
│   │       │
│   │       ├── connectivity/
│   │       │   ├── WiFiManager.h
│   │       │   ├── WiFiManager.cpp
│   │       │   ├── BleManager.h
│   │       │   └── BleManager.cpp
│   │       │
│   │       ├── storage/
│   │       │   ├── LittleFsLogRepository.h
│   │       │   ├── LittleFsLogRepository.cpp
│   │       │   ├── PreferencesConfigRepository.h
│   │       │   └── PreferencesConfigRepository.cpp
│   │       │
│   │       └── alert/
│   │           ├── M5AlertService.h
│   │           └── M5AlertService.cpp
│   │
│   ├── devtools/                       (*) chỉ build khi VC_ENABLE_DEVTOOLS
│   │   └── src/
│   │       ├── SerialMirror.h / .cpp       mirror canvas qua Serial (gói v2)
│   │       ├── SerialRemote.h / .cpp       lệnh Serial -> AppEvent (nút ảo)
│   │       └── TelemetryPrinter.h / .cpp   log số liệu + dòng @DATA
│   │
│   └── ui/
│       └── src/
│           ├── UiController.h
│           ├── UiController.cpp
│           ├── Theme.h
│           ├── assets/
│           │   ├── Icons.h
│           │   └── Fonts.h
│           └── screens/
│               ├── Screen.h                (*)
│               ├── BootScreen.h
│               ├── HomeScreen.h
│               ├── MeasuringScreen.h       (dùng chung cho Temperature / HeartRate / SpO2)
│               ├── ResultScreen.h
│               ├── ConnectionScreen.h
│               ├── SettingsScreen.h
│               └── DiagnosticScreen.h
│
├── tools/                              (*) công cụ chạy trên máy tính
│   ├── vc_protocol.py                  giao thức frame / @DATA dùng chung
│   ├── vc_simulator.py                 app Tkinter: mirror màn hình + nút A/B ảo
│   ├── canvas_capture.py               Serial monitor + chụp PNG
│   └── requirements.txt
│
└── test/
    ├── test_app_state/
    ├── test_time_service/
    └── test_measurement_service/

---

## Ghi chú

`(*)` là phần bổ sung so với cấu trúc ban đầu:

- **`ports/IInputSource.h`**: nút thật (`StickS3Board`) và lệnh Serial từ simulator (`SerialRemote`) đều sinh ra `AppEvent` qua cùng một port. Nhờ vậy `App` không phân biệt được nút thật với nút ảo.
- **`board/I2cBusManager`**: RTC (Grove G9/G10) và Hat2 (G7/G6) dùng chung `Wire`, nên cần chuyển bus trước mỗi lần đọc. Các provider dùng chung một instance.
- **`screens/Screen.h`**: lớp cơ sở của các screen, chứa các hàm vẽ tiêu đề và footer dùng chung.
- **`MeasuringScreen`**: một lớp có tham số `Metric` thay cho ba trang đo riêng, vì Heart Rate và SpO2 gần như trùng code.
- **`lib/devtools/`**: công cụ phát triển (mirror, nút ảo, `@DATA`), tách khỏi `ui` và `sticks3_platform` để bản build cho sản phẩm có thể tắt bằng `VC_ENABLE_DEVTOOLS=0`.
- **`tools/`**: script Python chạy trên máy tính, không build vào firmware.
- **`ports/ISensorProvider.h`** có thêm `reportStatus(DeviceStatus&)`, để mỗi cảm biến tự báo trạng thái của mình (SEN đã khởi tạo chưa, lần đọc MAX gần nhất có thành công không).

Hiện mới có những phần mà code đang dùng. Các file còn lại trong cây trên (BLE, LittleFS, WiFi/NTP, alert, Settings/Connection/Diagnostic/Result screen, MockSensorProvider) sẽ thêm ở các đợt sau.
# Kịch bản video giới thiệu thiết bị VitalCare StickS3

- **Thời lượng dự kiến:** khoảng 8 phút (khoảng 1.300 từ lời thoại, tốc độ nói vừa phải).
- **Cách đọc:** cột **Hình ảnh** gợi ý cảnh quay hoặc hình minh họa; **Lời thoại** là phần đọc.
- **Hình minh họa có sẵn:** `docs/hardware/*.svg` và ảnh chụp trong `docs/` (xem [HARDWARE.md](HARDWARE.md)).

---

## Mở đầu (0:00 – 0:30)

**Hình ảnh:** Thiết bị đeo trên cổ tay, màn hình hiện đồng hồ. Cắt sang logo VitalCare.

**Lời thoại:**

> Xin chào, chúng em là nhóm VitalCare, đồ án SEP490. Hệ thống VitalCare gồm ba tầng: trang web quản trị phòng khám, ứng dụng di động, và thiết bị IoT đeo tay đo sinh hiệu. Trong video này, chúng em giới thiệu tầng thứ ba: thiết bị VitalCare StickS3. Chúng em sẽ đi qua các thành phần phần cứng, nguyên lý đo, cách đi dây, và cuối cùng là cách sử dụng.

---

## 1. Tổng quan (0:30 – 1:15)

**Hình ảnh:** Hình `01_devices.svg`, lần lượt làm nổi bật từng khối.

**Lời thoại:**

> Thiết bị được ghép từ ba khối chính.
>
> Khối thứ nhất là **M5StickS3**, bộ điều khiển trung tâm có sẵn màn hình, pin và Bluetooth.
>
> Khối thứ hai là **module đồng hồ thời gian thực RTC8563** của M5Stack, giúp thiết bị luôn biết giờ chính xác.
>
> Khối thứ ba là **một bo mạch do nhóm tự hàn**, mang hai cảm biến: **SEN-15219** đo nhịp tim và SpO2, và **MAX30205** đo nhiệt độ.
>
> Ba khối được xếp chồng lên nhau thành một thiết bị nhỏ gọn đeo trên cổ tay.

---

## 2a. M5StickS3 (1:15 – 2:30)

**Hình ảnh:** Ảnh `anh_chi_tiet_thiet_bi_m5stick_o_ca_2_mat.webp`, sau đó quay cận thiết bị thật.

**Lời thoại:**

> M5StickS3 do hãng **M5Stack** sản xuất. Đây là hãng phần cứng chuyên làm các module lập trình nhỏ gọn, dạng lắp ghép.
>
> Bên trong là chip **ESP32-S3** của Espressif: bộ xử lý hai nhân 240 MHz, 8 MB bộ nhớ flash, 8 MB RAM mở rộng, có sẵn **Wi-Fi** và **Bluetooth Low Energy**.
>
> Thiết bị có màn hình màu 1,14 inch, pin 250 mAh sạc qua cổng USB-C, ba nút bấm, cảm biến chuyển động BMI270, micro và loa. Kích thước chỉ 48 × 24 × 15 mm, xấp xỉ một bao diêm.
>
> Nhóm chọn M5StickS3 vì bốn lý do:
>
> - **Thứ nhất, đủ nhỏ để đeo tay**, lại có sẵn pin, mạch sạc và vỏ, nên nhóm không phải tự làm phần này.
> - **Thứ hai, có Bluetooth Low Energy**: kiểu kết nối tiết kiệm pin mà đồng hồ thông minh dùng để nói chuyện với điện thoại.
> - **Thứ ba, có màn hình**, nên người bệnh xem được chỉ số ngay trên tay, không cần mở điện thoại.
> - **Thứ tư, dễ mở rộng**: cổng **HAT2** 16 chân ở đầu trên dùng để gắn cảm biến, cổng **Grove** ở đầu dưới để gắn module. Cảm biến chuyển động có sẵn cũng mở ra hướng phát triển sau này như phát hiện té ngã.

---

## 2b. Module RTC8563 (2:30 – 3:15)

**Hình ảnh:** Ảnh `anh_chup_mat_truoc_RTC_va_so_do_port_rtc.jpg` và `anh_spec_rtc.png`.

**Lời thoại:**

> Module thứ hai là **RTC Unit** của M5Stack, dùng chip đồng hồ **HYM8563**. RTC là viết tắt của Real-Time Clock, nghĩa là đồng hồ thời gian thực.
>
> Module có **thạch anh 32,768 kHz** để đếm giây chính xác, và **pin dự phòng riêng**, nên vẫn chạy khi thiết bị tắt hoặc hết pin.
>
> Vì sao cần RTC? Đồng hồ bên trong ESP32 sẽ về 0 mỗi lần khởi động lại. Trong khi đó, mỗi số đo sinh hiệu phải gắn đúng thời điểm đo, thì bác sĩ mới xem được diễn biến theo thời gian. Thiết bị lại thường xuyên đo khi **không có mạng**, nên không thể lấy giờ từ Internet.
>
> RTC giải quyết đúng vấn đề này. Điện thoại chỉ cần chỉnh giờ cho thiết bị một lần qua Bluetooth, sau đó RTC tự giữ giờ.
>
> Module nối với M5StickS3 qua cáp Grove 4 dây: đen là GND, đỏ là nguồn 5V, vàng là SDA ở chân G9, trắng là SCL ở chân G10.

---

## 2c. Bo mạch cảm biến SEN-15219 và MAX30205 (3:15 – 6:15)

### SEN-15219: nhịp tim và SpO2 (3:15 – 4:30)

**Hình ảnh:** Cận bo mạch đỏ SparkFun; hình minh họa ánh sáng chiếu vào da.

**Lời thoại:**

> SEN-15219 là cảm biến đo **nhịp tim và độ bão hòa oxy trong máu (SpO2)** của hãng SparkFun. Trên bo mạch có hai chip:
>
> - **MAX30101** là cảm biến quang: có đèn LED đỏ, hồng ngoại, xanh lá, và một tế bào thu ánh sáng.
> - **MAX32664** là chip xử lý, chạy sẵn thuật toán của hãng Maxim, trả về luôn nhịp tim, SpO2 và độ tin cậy của phép đo.
>
> Cảm biến hoạt động theo phương pháp **quang thể tích, gọi tắt là PPG**. Đèn LED chiếu ánh sáng vào da. Mỗi nhịp tim đẩy thêm máu vào mạch máu, lượng ánh sáng bị hấp thụ thay đổi theo, nên tín hiệu ánh sáng phản xạ dao động theo từng nhịp đập. Đếm số dao động trong một phút ta có **nhịp tim**.
>
> Còn **SpO2** thì dựa vào việc máu giàu oxy và máu thiếu oxy hấp thụ ánh sáng đỏ và hồng ngoại khác nhau. So sánh tỉ lệ hai loại ánh sáng này, cảm biến tính được phần trăm oxy trong máu.
>
> Vì đo bằng ánh sáng, phép đo **cần tay giữ yên**: cử động mạnh làm nhiễu tín hiệu. Thường mất 30 giây đến 1 phút để có kết quả ổn định.
>
> Về đi dây, SEN-15219 cần **6 chân**:
>
> - **3V3 và GND** để cấp nguồn.
> - **SDA và SCL** để truyền dữ liệu.
> - **RST** nối chân G1, dây tím, để khởi động lại chip xử lý.
> - **MFIO** nối chân G8, dây xanh dương, để chọn chế độ hoạt động.
>
> Địa chỉ I2C của cảm biến này là 0x55.

### MAX30205: nhiệt độ (4:30 – 5:30)

**Hình ảnh:** Cận bo mạch tím; hình mặt cắt cảm biến áp vào da.

**Lời thoại:**

> MAX30205 là cảm biến **nhiệt độ dành riêng cho đo thân nhiệt**, do Maxim, nay là Analog Devices, sản xuất. Sai số chỉ **±0,1 °C** trong khoảng 37 đến 39 °C, độ phân giải khoảng **0,004 °C**.
>
> Đây là cảm biến bán dẫn có sẵn bộ chuyển đổi số. Khi áp sát da, con chip ấm lên bằng da, và nó đo chính nhiệt độ đó rồi gửi về dạng số.
>
> Cần lưu ý: cảm biến đặt ở mu cổ tay đo **nhiệt độ da**, thường thấp hơn thân nhiệt khoảng 2 đến 3 °C. Vì vậy firmware của nhóm ước tính nhiệt độ cơ thể theo hai bước:
>
> - **Dự đoán nhiệt độ da:** khi mới đeo, da dưới cảm biến cần khoảng 15 phút để ấm hẳn. Để có số ngay sau 1 phút như đồng hồ thông minh, firmware đo tốc độ tăng của nhiệt độ da và dự đoán giá trị lúc ổn định. Mô hình này được rút ra từ dữ liệu đeo thật của nhóm.
> - **Cộng độ lệch:** cộng thêm độ lệch giữa da và cơ thể. Độ lệch hiện là 2,2 °C, có được nhờ so sánh với nhiệt kế đo miệng. Người dùng có thể hiệu chuẩn lại từ ứng dụng.
>
> Khi nhiệt độ da đã ổn định, con số chuyển sang màu xanh, nghĩa là kết quả đã chắc chắn.
>
> Về đi dây, MAX30205 dùng **4 dây**: VCC dây đỏ nối 3V3_L2, GND dây đen, SDA dây xanh lá nối G7, SCL dây vàng nối G6. Ba chân địa chỉ A0, A1, A2 nối xuống GND, tạo địa chỉ I2C 0x48. Chân cảnh báo OS để trống.
>
> Điểm đặc biệt trong cách đi dây của nhóm: **bốn dây nguồn và dữ liệu của SEN-15219 được nối chung tại MAX30205**, rồi chỉ một bó dây đi lên cổng HAT2. Nhờ vậy số dây giảm đi, thiết bị gọn hơn.

**Hình ảnh:** Hình `04_wiring.svg`.

### SDA và SCL là gì? (5:30 – 6:15)

**Hình ảnh:** Hình `03_hat_port.svg`; hình minh họa nhiều thiết bị trên cùng 2 dây.

**Lời thoại:**

> Từ nãy chúng em nhắc nhiều tới SDA và SCL. Đây là hai dây của chuẩn giao tiếp **I2C**:
>
> - **SDA** (Serial Data) là đường **dữ liệu**, nơi các con số đi qua lại.
> - **SCL** (Serial Clock) là đường **nhịp đồng hồ**, giúp hai bên gửi và nhận đúng nhịp.
>
> Có thể hình dung I2C như **một đường dây điện thoại chung**: nhiều thiết bị cùng nối vào hai dây, mỗi thiết bị có một "số điện thoại" riêng gọi là địa chỉ. Bộ điều khiển gọi đúng địa chỉ nào thì thiết bị đó trả lời.
>
> I2C cần thiết vì nó cho phép **nối nhiều cảm biến mà chỉ tốn hai chân tín hiệu**. Trong thiết bị có hai đường I2C:
>
> - **Đường HAT2**, gồm G7 và G6: MAX30205 ở địa chỉ 0x48 và SEN-15219 ở địa chỉ 0x55 dùng chung.
> - **Đường Grove**, gồm G9 và G10: dành cho RTC ở địa chỉ 0x51.
>
> Firmware chuyển qua lại giữa hai đường mỗi lần đọc.

---

## 2d. Lắp ráp, khởi động và hướng dẫn sử dụng (6:15 – 7:45)

**Hình ảnh:** Hình `02_layers.svg`, sau đó quay thao tác thật.

**Lời thoại:**

> Về lắp ráp, ba khối được xếp tầng từ trên xuống:
>
> - **Trên cùng là M5StickS3** gắn trên giá đeo.
> - **Ở giữa là RTC**, dán vào mặt lưng bằng băng dính hai mặt và nối bằng cáp Grove.
> - **Dưới cùng là bo mạch cảm biến**, dán vào RTC và hướng về phía da.
>
> Sáu dây từ cổng HAT2 đi xuống bo mạch cảm biến: đỏ, đen, xanh lá, vàng, tím, xanh dương.

**Hình ảnh:** Bấm nút nguồn; màn hình khởi động; log tự kiểm tra trên máy tính (tùy chọn).

> Khi bấm nút nguồn, thiết bị **tự kiểm tra phần cứng**: kiểm tra đường I2C và tìm đủ hai cảm biến, khởi tạo cảm biến nhịp tim mất khoảng 5 giây, rồi bật Bluetooth. Sau đó màn hình hiện **trang đồng hồ**.

**Hình ảnh:** Quay cận tay bấm nút, màn hình đổi trang.

> Cách sử dụng rất đơn giản:
>
> - **Nút A** (nút xanh mặt trước) chuyển lần lượt các trang: **nhiệt độ cơ thể, nhịp tim, SpO2**.
> - **Nút B** quay về trang đồng hồ.
> - **Góc trên bên phải** luôn hiện **phần trăm pin**, và **biểu tượng Bluetooth** khi đang kết nối điện thoại: màu xanh dương khi kết nối, màu cam khi đang đồng bộ.
>
> Khi mới đeo, trang nhiệt độ hiện **"Measuring"** trong khoảng 1 phút, sau đó hiện ngay nhiệt độ cơ thể. Lúc đầu con số màu trắng, kèm chữ **"settling"** vì đây là số tạm thời. Khi nhiệt độ ổn định, con số chuyển sang **màu xanh**. Nếu tháo thiết bị ra, màn hình báo **"NOT WORN"**.
>
> Với nhịp tim và SpO2, người dùng giữ tay yên để cảm biến áp sát da, chờ khoảng 30 giây đến 1 phút để số đo hiện lên.

**Hình ảnh:** Màn hình điện thoại hoặc máy tính nhận dữ liệu realtime; lệnh đồng bộ.

> Về dữ liệu, thiết bị làm hai việc song song:
>
> - **Gửi realtime:** khi có điện thoại kết nối, mỗi giây gửi một gói số đo qua Bluetooth để hiển thị trực tiếp.
> - **Lưu offline:** dù có kết nối hay không, cứ 10 giây thiết bị ghi một bản ghi trung bình vào bộ nhớ trong, đủ cho khoảng 6 ngày.
>
> Khi điện thoại kết nối lại, dữ liệu được **đồng bộ theo từng gói có xác nhận**: thiết bị chỉ xóa dữ liệu sau khi điện thoại báo đã lưu thành công, nên **không mất số đo** khi mất kết nối giữa chừng.
>
> Thiết bị sạc qua cổng USB-C, bấm đúp nút nguồn bên hông để tắt.

---

## Kết (7:45 – 8:15)

**Hình ảnh:** Thiết bị trên tay; chuyển sang sơ đồ ba tầng của hệ thống VitalCare.

**Lời thoại:**

> Tóm lại, VitalCare StickS3 kết hợp ba khối:
>
> - **M5StickS3** để xử lý, hiển thị và kết nối.
> - **RTC** để giữ thời gian chính xác.
> - **Bo mạch cảm biến** đo nhiệt độ, nhịp tim và SpO2.
>
> Dữ liệu từ thiết bị được gửi về ứng dụng di động, rồi lên hệ thống của phòng khám để bác sĩ và người thân theo dõi.
>
> Hướng phát triển tiếp theo của nhóm là **đo định kỳ kết hợp ngủ sâu** để pin dùng được 3 đến 4 ngày, thêm cảnh báo khi chỉ số vượt ngưỡng, và làm mạch in thay cho dây hàn tay để thiết bị bền hơn.
>
> Cảm ơn thầy cô và các bạn đã theo dõi.

---

## Ghi chú cho người quay

- **Xác nhận vị trí đo nhịp tim:** SEN-15219 vốn thiết kế để đo ở **đầu ngón tay**, và màn hình hiện "PLACE FINGER" khi chưa có tín hiệu. Nếu khi quay demo cần đặt ngón tay lên cảm biến thay vì đo ở cổ tay, hãy sửa lời thoại mục 2d cho đúng thao tác thật.
- **Quay màn hình:** quay trực tiếp màn hình thiết bị. Nếu cần hình sắc nét để chèn vào video, có thể chụp bằng `tools/canvas_capture.py` hoặc phản chiếu qua `tools/vc_simulator.py`. Lưu ý mở cổng Serial sẽ làm thiết bị khởi động lại, nên mở trước khi bắt đầu quay.
- **Nếu cần rút xuống 5 phút:** bỏ phần giải thích SpO2 (mục 2c) và phần dữ liệu (mục 2d), chỉ giữ ý "thiết bị gửi realtime và lưu offline".

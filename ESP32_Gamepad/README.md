# STM32 Game Controller

Nạp firmware STM32 bằng project Keil trong `STM32_Game_Controller/MDK-ARM`. Nạp `ESP32_Gamepad.ino` bằng Arduino IDE với Arduino core **esp32** của Espressif. Với kit ESP32-WROOM-32D, chọn **ESP32 Dev Module**. Sketch dùng **ESP32-BLE-Gamepad** và **NimBLE-Arduino**. Cấu hình UART dưới đây dành cho ESP32 DevKit có GPIO16 và GPIO17 khả dụng.

| STM32 | Thiết bị | Kết nối |
|---|---|---|
| PA0, PA1 | Joystick | X, Y |
| PA8 | Nút joystick | Đổi chế độ OLED / PC |
| PB0, PB1, PB10, PB11 | Nút chơi | A, B, C, D theo thứ tự, nhấn nối GND |
| PB6, PB7 | SSD1306 128 × 64 | SCL, SDA |
| PA9 (TX), PA10 (RX) | ESP32 | GPIO16 (RX), GPIO17 (TX) |
| GND | ESP32 | GND chung, UART mức 3,3 V |

Khi bật nguồn, OLED hiện màn hình Né vật cản. Nhấn A để bắt đầu, gạt joystick trái hoặc phải để đổi làn. Trả cần về giữa trước khi gạt tiếp. Mỗi vật cản vượt qua được tính một điểm. Khi va chạm, nhấn A để chơi lại.

Nhấn PA8 để vào chế độ PC và ghép đôi với thiết bị **STM32 XY Pad** trong Windows. Nếu cùng ESP32 đã từng chạy firmware có sơ đồ HID khác, hãy xóa ghép đôi cũ trước khi ghép đôi lại. Trong `joy.cpl`, tay cầm khai báo hai trục X/Y và bốn nút. OLED hiện trạng thái đường kết nối BLE do ESP32 báo về. Khi quay lại OLED, ESP32 có thể giữ kết nối BLE và gửi trạng thái trung tính để nhả các nút. Chọn game PC hỗ trợ tay cầm HID phổ thông.

## Luồng dữ liệu

- `task_input` đọc joystick và chống dội nút mỗi 10 ms. ADC1 dùng nhóm injected để hai mẫu nằm trong hai thanh ghi kết quả riêng. Cấu hình nhóm regular do CubeMX sinh được giữ nguyên.
- `task_game` đọc bản chụp đầu vào mỗi 40 ms và xuất trạng thái màn hình hoặc game. Bộ đếm lần nhấn giữ được sự kiện PA8 và A ngay cả khi task game đọc chậm hơn task input.
- `task_display` đọc trạng thái game và BLE. SSD1306 nhận lần lượt từng trang để một lần truyền không chiếm toàn bộ thời gian cập nhật màn hình.
- `task_comm` trao đổi với ESP32 mỗi 20 ms qua USART1, 115200 8N1. Gói gồm `FF MODE X Y BUTTONS CHECK`. Trục được cộng 127 khi truyền. `CHECK` là XOR của bốn byte dữ liệu, lấy 7 bit thấp. ESP32 trả `C1` khi có kết nối BLE hoặc `C0` khi đang chờ. ESP32 đưa tay cầm về trung tính nếu mất dữ liệu UART từ 200 ms trở lên.

Giá trị giữa joystick đang đặt ở 2048, với vùng chết ±160 số đếm ADC. Nếu cần chỉnh theo joystick thực tế, các hằng số nằm trong `app/src/task_input.c`.


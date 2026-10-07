# Lộ trình thiết kế lại Giao diện Màn hình TFT (STM32)

Dựa trên mã nguồn hiện tại của hệ thống (sử dụng màn hình TFT chip ST7789, giao diện Landscape độ phân giải khoảng 320x240, và đang vẽ bằng các hàm cơ bản trong `main.c`), dưới đây là lộ trình chi tiết để nâng cấp toàn diện giao diện (UI/UX) cho thiết bị nhúng của bạn.

## Giai đoạn 1: Phân tích & Thiết kế UX/UI (Mockup)

Trước khi code, cần định hình lại cách bố trí thông tin trên màn hình 320x240.

- **Bố cục khung (Layout Structure):** Thay vì các thành phần trôi nổi, hãy chia màn hình thành 3 vùng chuẩn:
  - **Header (Cao 30px):** Trạng thái hệ thống (Icon MQTT, Icon Pin, Trạng thái sạc/xả, Giờ).
  - **Main Content (Cao 180px):** Khu vực hiển thị thông tin chính của từng trang.
  - **Footer/Navbar (Cao 30px):** Chỉ báo trang hiện tại (VD: `[1] 2 3` hoặc các chấm tròn `o O o`) để người dùng biết họ đang ở đâu.
- **Màu sắc (Color Palette):** Giữ nền đen (Dark Mode) để tiết kiệm năng lượng và tăng độ tương phản. Bổ sung hệ thống màu: 
  - *Accent:* Cyan (Điện áp), Neon Green (SOC/Dòng điện), Magenta/Blue (SOH/Digital Twin).
  - *Alerts:* Red/Orange (Quá nhiệt, Lỗi).
- **Trải nghiệm chuyển trang:** Chuyển từ việc tự động nhảy trang mỗi 5s sang:
  - Hiển thị một thanh tiến trình (progress bar) mỏng ở góc dưới để báo hiệu sắp đổi trang.
  - Hoặc tích hợp nút nhấn vật lý (GPIO) để người dùng chủ động chuyển trang.

## Giai đoạn 2: Tối ưu hóa kiến trúc Vẽ (Graphics Rendering)

Mã nguồn hiện tại gọi `ST7789_Fill_Color(BLACK)` mỗi khi đổi trang gây nháy (flicker) màn hình. Cần tối ưu:

- **Vẽ cục bộ (Partial Redraw):** Chỉ ghi đè (xóa) vùng chứa con số bị thay đổi thay vì xóa toàn màn hình. Khi in số mới, dùng hàm in chuỗi có nền (Background color) hoặc xóa riêng hcn nhỏ chứa số cũ.
- **Tách Module UI:** Tách toàn bộ logic vẽ giao diện từ `main.c` ra các file riêng `User/UI/ui_main.c`, `ui_main.h`. Giúp code gọn gàng và dễ mở rộng.
- **State Machine cho UI:** Chuyển logic cập nhật giao diện thành máy trạng thái, quản lý các event như `UI_EVENT_PAGE_NEXT`, `UI_EVENT_DATA_UPDATE`.

## Giai đoạn 3: Phát triển các UI Component nâng cao (Widget)

Nâng cấp các hình khối cơ bản thành các widget trực quan:

- **Circular Gauge / Progress Bar:** Lập trình hàm vẽ cung tròn hoặc vòng tròn khuyết để hiển thị SOC% hoặc SOH%, nhìn hiện đại hơn rất nhiều so với con số thô.
- **Bar Chart cho Cells:** Thay vì in số Cell 1, 2, 3, 4, hãy vẽ 4 cột đứng xếp cạnh nhau. Cột đầy hay vơi tương ứng với phần trăm của cell đó -> Dễ dàng nhìn thấy ngay hiện tượng mất cân bằng (Cell imbalance).
- **Icons Bitmap:** Dùng các công cụ (như Image2LCD) để chuyển đổi các icon PNG nhỏ (Nhiệt kế, Cảnh báo, AI) thành mảng mã HEX (C array) và tạo hàm `ST7789_DrawBitmap()` để hiển thị. 
- **Bo góc (Rounded Cards):** Viết hàm `ST7789_DrawRoundRect()` tạo các tấm thẻ (panel) chứa dữ liệu thay vì khung vuông góc cạnh.

## Giai đoạn 4: Quy hoạch lại luồng nội dung các trang (Page Flow)

**Trang 1: Dashboard Tổng quan (Cái nhìn nhanh)**
- Tập trung vào 4 thông số lớn nhất, đặt trong 4 ô card: **Pack V**, **Pack A**, **Nhiệt độ**, **SOC (Kèm Icon Pin lớn)**.
- Trạng thái chữ to: `DISCHARGING` / `CHARGING`.

**Trang 2: Chi tiết các Cells & Sức khỏe Pin**
- Biểu đồ cột Bar chart của 4 Cell.
- Dưới cùng hiển thị độ lệch áp (Delta V).
- Các thông số SOH% và Tổng dung lượng (Total Ah).

**Trang 3: Edge AI & Diagnostics (Chẩn đoán thông minh)**
- Trạng thái AI: Healthy / Degraded / Anomalies.
- Chu kì pin (Cycles).
- Cảnh báo an toàn (nhiệt độ quá cao, áp quá thấp...).

## Giai đoạn 5: Triển khai và Testing

- **Bước 1:** Bổ sung các hàm vẽ hình học mới (Vẽ hình tròn, Bar chart, Icon) vào thư viện `st7789.c`.
- **Bước 2:** Xây dựng file `ui.c` với các hàm vẽ khung tĩnh (chỉ vẽ 1 lần khi chuyển trang).
- **Bước 3:** Xây dựng hàm cập nhật dữ liệu động `UI_UpdateData()` (chỉ update số liệu mỗi 1s).
- **Bước 4:** Nạp code và kiểm tra độ mượt mà, tốc độ khung hình, và đảm bảo không bị lỗi tràn RAM.

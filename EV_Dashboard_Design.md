# Thiết kế Giao diện Ô tô (EV Dashboard) cho STM32

Để đạt được giao diện chuẩn "xe điện" (EV Dashboard) trên vi điều khiển STM32F103, chúng ta cần một bố cục đối xứng, sử dụng cung tròn (Gauge) và màu sắc tương phản cao (Đen - Xanh Neon - Cyan).

## 1. Bản vẽ Ý tưởng (Mockup)

Dưới đây là bản thiết kế ý tưởng tôi đã tạo ra. Nó mang phong cách màn hình ô tô điện hiện đại.

````carousel
![EV Dashboard Mockup](/C:/Users/PC/.gemini/antigravity/brain/6e6af785-ae96-4fcc-a1df-da7c7c980ac7/ev_dashboard_mockup_1791361213757.jpg)
<!-- slide -->
### Cấu trúc Layout trên STM32 (320x240)

- **Trung tâm (Gauge lớn):** Hiển thị vòng cung % SOC (Dung lượng pin). Chữ số % thật to ở giữa.
- **Bên Trái:** Hiển thị Pack Voltage (Điện áp tổng) với font chữ màu Xanh lá/Cyan.
- **Bên Phải:** Hiển thị Current (Dòng điện) với font chữ lớn.
- **Header:** Giờ, Trạng thái hệ thống (Discharging/Charging).
- **Footer:** Nhiệt độ, AI Health, SOH.
````

## 2. Làm sao để vẽ "Vòng cung tốc độ" (Gauge) mượt mà trên STM32F103?

STM32F103 không có bộ tăng tốc đồ họa phần cứng (như Chrom-ART) và bộ nhớ RAM nhỏ, việc dùng toán học `sin()` `cos()` để tính toán từng điểm ảnh cho 1 vòng cung dày sẽ **rất chậm và giật**. 

**Giải pháp (Thuật toán "Ring & Cut"):**
Thay vì vẽ vòng cung, chúng ta làm 3 bước cực nhanh:
1. Vẽ 1 hình tròn đặc (Filled Circle) to, màu Xanh Neon.
2. Vẽ 1 hình tròn đặc nhỏ hơn một chút, màu Đen (Black) đè lên ngay chính giữa -> Ta có 1 vòng nhẫn (Ring).
3. Vẽ 1 hình chữ nhật đặc (Filled Rectangle) màu Đen đè lên nửa dưới của vòng nhẫn -> Ta có một cung tròn nửa vòng (Semi-circle Arc) hoàn hảo và cực kỳ mượt mà!

Tiến trình sạc/xả của vòng cung sẽ được điều khiển bằng cách vẽ đè các đường nét (line) từ tâm tỏa ra.

## 3. Kiến trúc Mã nguồn C (Dự kiến)

Chúng ta sẽ tạo các file mới `User/UI/ui_dashboard.c` và `ui_dashboard.h`.

```c
// ui_dashboard.h
#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include "st7789.h"
#include "Battery.h"
#include "Temperature.h"

// Màu sắc mang phong cách xe hơi
#define COLOR_NEON_GREEN 0x07E0
#define COLOR_CYAN       0x07FF
#define COLOR_DARK_GRAY  0x3186
#define COLOR_ALERT_RED  0xF800

void UI_InitDashboard(void);
void UI_UpdateDashboard(void);

#endif
```

```c
// ui_dashboard.c
#include "ui_dashboard.h"
#include <stdio.h>

static uint8_t last_soc = 255; // Lưu trạng thái cũ để chỉ vẽ lại khi thay đổi

// Hàm vẽ vòng cung nhanh (Hack cách vẽ)
void DrawSemiCircleGauge(uint16_t x, uint16_t y, uint16_t r_outer, uint16_t r_inner, uint16_t color) {
    // 1. Vẽ hình tròn to
    ST7789_DrawFilledCircle(x, y, r_outer, color);
    // 2. Xoá tâm để tạo vòng nhẫn
    ST7789_DrawFilledCircle(x, y, r_inner, BLACK);
    // 3. Xóa nửa dưới để tạo vòng cung nửa (Speedometer)
    ST7789_DrawFilledRectangle(x - r_outer, y, r_outer * 2 + 1, r_outer + 1, BLACK);
}

void UI_InitDashboard(void) {
    ST7789_Fill_Color(BLACK);
    
    // Vẽ Header
    ST7789_WriteString(10, 5, "EV DRIVE MODE", Font_7x10, COLOR_CYAN, BLACK);
    ST7789_DrawLine(0, 18, 320, 18, COLOR_DARK_GRAY);
    
    // Khung viền 2 bên
    ST7789_WriteString(10, 80, "VOLTAGE", Font_7x10, COLOR_DARK_GRAY, BLACK);
    ST7789_WriteString(250, 80, "CURRENT", Font_7x10, COLOR_DARK_GRAY, BLACK);
    
    // Vẽ khung vòng cung ở giữa (Nền xám)
    DrawSemiCircleGauge(160, 140, 80, 60, COLOR_DARK_GRAY);
}

void UI_UpdateDashboard(void) {
    char buf[16];
    
    // Chỉ cập nhật SOC nếu thay đổi
    if (Battery.SOC != last_soc) {
        last_soc = Battery.SOC;
        
        // Cập nhật vòng cung màu tùy theo % 
        // (Đây là logic demo, thực tế sẽ tính góc để lấp đầy màu)
        uint16_t gaugeColor = (Battery.SOC > 20) ? COLOR_NEON_GREEN : COLOR_ALERT_RED;
        DrawSemiCircleGauge(160, 140, 80, 60, gaugeColor); 
        
        // Cập nhật số to ở giữa
        sprintf(buf, "%3d%%", Battery.SOC);
        ST7789_WriteString(130, 100, buf, Font_16x26, WHITE, BLACK);
    }
    
    // Cập nhật áp dòng liên tục (Vẽ đè cục bộ)
    sprintf(buf, "%5.2f", Battery.Voltage);
    ST7789_WriteString(10, 100, buf, Font_11x18, COLOR_NEON_GREEN, BLACK);
    
    sprintf(buf, "%5.2f", Battery.Current);
    ST7789_WriteString(250, 100, buf, Font_11x18, COLOR_CYAN, BLACK);
}
```

## 4. Các bước tiếp theo để tích hợp vào `main.c`

1. **Khởi tạo:** Trong hàm `main()`, sau khi cấu hình xong thiết bị, gọi `UI_InitDashboard()`.
2. **Vòng lặp:** Xóa logic `switch` trang hiện tại và thay bằng lệnh gọi `UI_UpdateDashboard()` bên trong `while(1)`.
3. **Thêm Font lớn:** Nếu muốn chữ đẹp hơn nữa, ta sẽ cần dùng công cụ tạo Font (như PCtoLCD2002) để add thêm một `Font_24x36` hoặc lớn hơn chuyên để vẽ số.

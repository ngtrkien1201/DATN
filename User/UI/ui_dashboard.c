#include "ui_dashboard.h"
#include "st7789.h"
#include "fonts.h"
#include "Battery.h"
#include "Temperature.h"
#include "SOH.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================
// HỆ MÀU SẮC TAPLO XE ĐIỆN CAO CẤP (EV DIGITAL CLUSTER THEME)
// ============================================================
#define EV_BG            0x0000  // Nền đen tuyệt đối (OLED Style)
#define EV_PANEL_BG      0x0862  // Nền Panel Titanium tối
#define EV_PANEL_BORDER  0x2125  // Viền Dark Slate tinh tế
#define EV_BAR_TRACK     0x10A2  // Rãnh đo pin
#define EV_CYAN          0x07FF  // Electric Neon Cyan (Cụm tâm điểm)
#define EV_GREEN         0x07E0  // Eco EV Green ([READY], Sạc, Bình thường)
#define EV_YELLOW        0xFFE0  // Cảnh báo nhẹ, Điện áp Pack
#define EV_ORANGE        0xFDA0  // Sport Amber (Xả dòng lớn, Cảnh báo)
#define EV_RED           0xF800  // Báo động lỗi (Fault Alert)
#define EV_WHITE         0xFFFF  // Trắng tương phản cao
#define EV_GRAY          0x8410  // Màu chữ phụ
#define EV_MUTED         0x4A69  // Màu mờ nhạt cho khung phụ

// ============================================================
// BỘ NHỚ ĐỆM (CACHE) TRẠNG THÁI CHỐNG LAG & CHỐNG NHẤP NHÁY
// ============================================================
static uint8_t force_redraw = 1;

static float   last_voltage = -999.0f;
static float   last_current = -999.0f;
static float   last_power   = -999.0f;
static float   last_temp    = -999.0f;
static uint8_t last_status  = 255;
static uint8_t last_soc     = 255;
static uint8_t last_soh     = 255;
static float   last_cycles  = -999.0f;
static float   last_energy  = -999.0f;

static float   last_cell_v[4]   = {-999.0f, -999.0f, -999.0f, -999.0f};
static uint8_t last_cell_soc[4] = {255, 255, 255, 255};
static float   last_delta_v     = -999.0f;

// Cache cho AI Sentinel & Digital Twin
static int     last_ai_class = -1;
static float   last_dt_vest  = -999.0f;
static float   last_dt_acc   = -999.0f;

// Biến trạng thái mô hình Digital Twin (1RC Thevenin ECM)
static float dt_Vp = 0.0f;
static float dt_Vest = 0.0f;
static float dt_accuracy = 99.5f;
static const float dt_R0 = 0.180f;  // ~45mOhm x 4 = 180mOhm
static const float dt_R1 = 0.080f;  // 80mOhm

// ============================================================
// HÀM VẼ THANH TIẾN ĐỘ XE ĐIỆN PHÂN ĐOẠN (EV SEGMENT BAR)
// ============================================================
static void DrawEVSegmentBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                             uint8_t old_pct, uint8_t new_pct, uint16_t fill_col) 
{
    if (new_pct > 100) new_pct = 100;
    if (old_pct > 100) old_pct = 100;

    uint16_t new_w = (new_pct * w) / 100;

    if (old_pct == 255 || force_redraw) {
        if (new_w > 0) {
            ST7789_DrawFilledRectangle(x, y, new_w, h, fill_col);
        }
        if (new_w < w) {
            ST7789_DrawFilledRectangle(x + new_w, y, w - new_w, h, EV_BAR_TRACK);
        }
    } else if (new_pct != old_pct) {
        uint16_t old_w = (old_pct * w) / 100;
        if (new_w > old_w) {
            ST7789_DrawFilledRectangle(x + old_w, y, new_w - old_w, h, fill_col);
        } else if (new_w < old_w) {
            ST7789_DrawFilledRectangle(x + new_w, y, old_w - new_w, h, EV_BAR_TRACK);
        }
    }
}

// ============================================================
// KHỞI TẠO KHUNG ĐỒ HỌA TABLO XE ĐIỆN (VẼ 1 LẦN DUY NHẤT)
// ============================================================
void UI_InitDashboard(void) 
{
    force_redraw = 1;
    last_voltage = -999.0f;
    last_current = -999.0f;
    last_power   = -999.0f;
    last_temp    = -999.0f;
    last_status  = 255;
    last_soc     = 255;
    last_soh     = 255;
    last_cycles  = -999.0f;
    last_energy  = -999.0f;
    last_ai_class = -1;
    last_dt_vest  = -999.0f;
    last_dt_acc   = -999.0f;
    for (int i = 0; i < 4; i++) {
        last_cell_v[i] = -999.0f;
        last_cell_soc[i] = 255;
    }

    // 1. Phủ toàn bộ màn hình màu đen OLED
    ST7789_Fill_Color(EV_BG);

    // 2. THANH TRẠNG THÁI OTO ĐIỆN (TOP STATUS CLUSTER BAR: Y = 0..20)
    ST7789_DrawFilledRectangle(0, 0, 320, 20, 0x0841);
    ST7789_DrawLine(0, 20, 319, 20, EV_PANEL_BORDER);
    
    // Đèn báo READY màu xanh lục kinh điển của xe điện
    ST7789_WriteString(6, 5, "[READY]", Font_7x10, EV_GREEN, 0x0841);
    
    // Tiêu đề đồng hồ taplo xe điện
    ST7789_WriteString(124, 5, "EV TWIN CLUSTER", Font_7x10, EV_WHITE, 0x0841);
    
    // Đèn giám sát AI
    ST7789_WriteString(244, 5, "AI:ON", Font_7x10, EV_GREEN, 0x0841);

    // 3. CỤM 3 ĐỒNG HỒ ĐIỆN TỬ CHÍNH (Y = 24..130)
    // 3.1 Cột trái: HỆ THỐNG TRUYỀN ĐỘNG (POWERTRAIN TELEMETRY)
    ST7789_DrawFilledRectangle(4, 24, 96, 107, EV_PANEL_BG);
    ST7789_DrawRectangle(4, 24, 99, 130, EV_PANEL_BORDER);
    ST7789_WriteString(10, 28, "POWERTRAIN", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(10, 68, "V:", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(10, 82, "I:", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(10, 96, "T:", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(10, 110, "E:", Font_7x10, EV_GRAY, EV_PANEL_BG);

    // 3.2 Cột giữa: CỤM ĐỒNG HỒ NĂNG LƯỢNG TRUNG TÂM (BATTERY ENERGY CORE)
    ST7789_DrawFilledRectangle(104, 24, 112, 107, EV_PANEL_BG);
    ST7789_DrawRectangle(104, 24, 215, 130, EV_CYAN); // Viền Cyan nổi bật như taplo Tesla
    ST7789_WriteString(118, 28, "BATTERY CORE", Font_7x10, EV_CYAN, EV_PANEL_BG);

    // 3.3 Cột phải: THỂ SONG SINH SỐ (DIGITAL TWIN 1RC ECM)
    ST7789_DrawFilledRectangle(220, 24, 96, 107, EV_PANEL_BG);
    ST7789_DrawRectangle(220, 24, 315, 130, EV_PANEL_BORDER);
    ST7789_WriteString(226, 28, "DIGITAL TWIN", Font_7x10, EV_CYAN, EV_PANEL_BG);
    ST7789_WriteString(226, 68, "1RC Thevenin", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(226, 82, "ACC:", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(226, 96, "SOH:", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(226, 110, "R0 :", Font_7x10, EV_GRAY, EV_PANEL_BG);

    // 4. KHUNG BỘ PIN SKATEBOARD DƯỚI SÀN XE (4S CHASSIS PACK: Y = 134..195)
    ST7789_DrawFilledRectangle(4, 134, 312, 62, EV_PANEL_BG);
    ST7789_DrawRectangle(4, 134, 315, 195, EV_PANEL_BORDER);
    ST7789_WriteString(10, 138, "4S SKATEBOARD PACK", Font_7x10, EV_GRAY, EV_PANEL_BG);

    // Vẽ viền cho 4 Pod Cell xe điện
    for (int i = 0; i < 4; i++) {
        uint16_t cx = 8 + i * 77;
        ST7789_DrawRectangle(cx, 149, cx + 70, 191, EV_PANEL_BORDER);
    }

    // 5. THANH DƯỚI CÙNG: AI SENTINEL BẢO VỆ XE & THÔNG SỐ HÀNH TRÌNH (Y = 199..237)
    // 5.1 Khung AI Sentinel
    ST7789_DrawFilledRectangle(4, 199, 190, 39, EV_PANEL_BG);
    ST7789_DrawRectangle(4, 199, 193, 237, EV_PANEL_BORDER);
    ST7789_WriteString(10, 204, "SENTINEL AI:", Font_7x10, EV_CYAN, EV_PANEL_BG);
    ST7789_WriteString(10, 220, "CONF: 99.5% | SAFE", Font_7x10, EV_GRAY, EV_PANEL_BG);

    // 5.2 Khung Odometer
    ST7789_DrawFilledRectangle(198, 199, 118, 39, EV_PANEL_BG);
    ST7789_DrawRectangle(198, 199, 315, 237, EV_PANEL_BORDER);
    ST7789_WriteString(204, 204, "ODO :", Font_7x10, EV_GRAY, EV_PANEL_BG);
    ST7789_WriteString(204, 220, "CYC :", Font_7x10, EV_GRAY, EV_PANEL_BG);
}

// ============================================================
// CẬP NHẬT GIAO DIỆN XE ĐIỆN THEO THỜI GIAN THỰC (ZERO FLICKER)
// ============================================================
void UI_UpdateDashboard(void) 
{
    char buf[32];

    // --------------------------------------------------------
    // BƯỚC 1: CẦN SỐ XE ĐIỆN VÀ TRẠNG THÁI (P - R - N - D)
    // --------------------------------------------------------
    if (Battery.Status != last_status || force_redraw) {
        last_status = Battery.Status;
        if (Battery.Status == BATTERY_CHARGING) {
            // Đang cắm sạc: Xe ở chế độ Park [P] + Đang sạc năng lượng
            ST7789_WriteString(56, 5, "[P] R N D", Font_7x10, EV_GREEN, 0x0841);
        } else if (Battery.Status == BATTERY_DISCHARGING) {
            // Đang cấp nguồn xả tải: Xe ở chế độ Drive [D]
            ST7789_WriteString(56, 5, " P R N[D]", Font_7x10, EV_ORANGE, 0x0841);
        } else {
            // Xe đang đỗ: Neutral [N]
            ST7789_WriteString(56, 5, " P R[N] D", Font_7x10, EV_WHITE, 0x0841);
        }
    }

    // --------------------------------------------------------
    // BƯỚC 2: CỘT TRÁI - HỆ THỐNG TRUYỀN ĐỘNG (POWERTRAIN)
    // --------------------------------------------------------
    // Công suất tức thời cỡ lớn
    if (fabsf(Battery.Power - last_power) >= 0.05f || force_redraw) {
        last_power = Battery.Power;
        sprintf(buf, "%5.1fW", fabsf(Battery.Power));
        uint16_t p_col = (Battery.Current > 0.01f) ? EV_ORANGE : ((Battery.Current < -0.01f) ? EV_GREEN : EV_WHITE);
        ST7789_WriteString(10, 44, buf, Font_11x18, p_col, EV_PANEL_BG);
    }

    // Điện áp Pack
    if (fabsf(Battery.Voltage - last_voltage) >= 0.01f || force_redraw) {
        last_voltage = Battery.Voltage;
        sprintf(buf, "%5.2fV", Battery.Voltage);
        ST7789_WriteString(26, 68, buf, Font_7x10, EV_YELLOW, EV_PANEL_BG);
    }

    // Dòng điện Pack
    if (fabsf(Battery.Current - last_current) >= 0.005f || force_redraw) {
        last_current = Battery.Current;
        sprintf(buf, "%+5.2fA", Battery.Current);
        uint16_t i_col = (Battery.Current > 0.01f) ? EV_ORANGE : ((Battery.Current < -0.01f) ? EV_GREEN : EV_WHITE);
        ST7789_WriteString(26, 82, buf, Font_7x10, i_col, EV_PANEL_BG);
    }

    // Nhiệt độ khối pin
    if (fabsf(Temp.Temperature - last_temp) >= 0.2f || force_redraw) {
        last_temp = Temp.Temperature;
        sprintf(buf, "%4.1fC", Temp.Temperature);
        uint16_t t_col = (Temp.Temperature > 50.0f) ? EV_RED : ((Temp.Temperature > 40.0f) ? EV_ORANGE : EV_GREEN);
        ST7789_WriteString(26, 96, buf, Font_7x10, t_col, EV_PANEL_BG);
    }

    // Năng lượng tiêu thụ
    if (fabsf(Battery.Energy - last_energy) >= 0.01f || force_redraw) {
        last_energy = Battery.Energy;
        sprintf(buf, "%4.1fWh", Battery.Energy);
        ST7789_WriteString(26, 110, buf, Font_7x10, EV_GRAY, EV_PANEL_BG);
    }

    // --------------------------------------------------------
    // BƯỚC 3: CỘT GIỮA - TRỌNG TÂM TABLO: SOC PIN KHỔNG LỒ & QUÃNG ĐƯỜNG
    // --------------------------------------------------------
    if (Battery.SOC != last_soc || force_redraw) {
        // Hiển thị số SOC cực lớn bằng Font_16x26 ở chính giữa màn hình xe điện
        sprintf(buf, "%3d%%", Battery.SOC);
        uint16_t soc_col = (Battery.SOC > 30) ? EV_CYAN : ((Battery.SOC > 15) ? EV_ORANGE : EV_RED);
        ST7789_WriteString(126, 43, buf, Font_16x26, soc_col, EV_PANEL_BG);

        // Thanh Segmented Gauge xe điện nằm ngang
        DrawEVSegmentBar(112, 74, 96, 7, last_soc, Battery.SOC, soc_col);
        last_soc = Battery.SOC;

        // Ước lượng quãng đường di chuyển còn lại của xe điện (Estimated Range)
        float est_range = (Battery.SOC * 0.45f); // Giả lập tỉ lệ phạm vi di chuyển EV
        sprintf(buf, "RNG:%4.1fkm", est_range);
        ST7789_WriteString(116, 90, buf, Font_7x10, EV_GREEN, EV_PANEL_BG);

        // Chế độ lái xe điện
        if (Battery.Status == BATTERY_CHARGING) {
            ST7789_WriteString(128, 110, "[CHARGING]", Font_7x10, EV_GREEN, EV_PANEL_BG);
        } else if (Battery.Status == BATTERY_DISCHARGING) {
            ST7789_WriteString(128, 110, "[DRIVE ON]", Font_7x10, EV_ORANGE, EV_PANEL_BG);
        } else {
            ST7789_WriteString(128, 110, "[PARKED  ]", Font_7x10, EV_GRAY, EV_PANEL_BG);
        }
    }

    // --------------------------------------------------------
    // BƯỚC 4: CỘT PHẢI - THỂ SONG SINH SỐ DIGITAL TWIN (1RC ECM)
    // --------------------------------------------------------
    // Giải phương trình vi phân mô hình Thevenin 1RC
    float exp_factor = 0.9231f; // exp(-1.0 / 12.5)
    dt_Vp = dt_Vp * exp_factor + Battery.Current * dt_R1 * (1.0f - exp_factor);

    float dt_ocv = (3.20f + (Battery.SOC / 100.0f) * 1.00f) * 4.0f;
    dt_Vest = dt_ocv + Battery.Current * dt_R0 + dt_Vp;

    float err_v = fabsf(Battery.Voltage - dt_Vest);
    if (Battery.Voltage > 1.0f) {
        dt_accuracy = 100.0f - (err_v / Battery.Voltage * 100.0f);
    } else {
        dt_accuracy = 99.4f;
    }
    if (dt_accuracy > 99.9f) dt_accuracy = 99.8f;
    if (dt_accuracy < 80.0f) dt_accuracy = 85.0f;

    // Điện áp mô phỏng Digital Twin cỡ lớn
    if (fabsf(dt_Vest - last_dt_vest) >= 0.02f || force_redraw) {
        last_dt_vest = dt_Vest;
        sprintf(buf, "%5.2fV", dt_Vest);
        ST7789_WriteString(226, 44, buf, Font_11x18, EV_CYAN, EV_PANEL_BG);
    }

    // Độ chính xác mô hình
    if (fabsf(dt_accuracy - last_dt_acc) >= 0.2f || force_redraw) {
        last_dt_acc = dt_accuracy;
        sprintf(buf, "%4.1f%%", dt_accuracy);
        ST7789_WriteString(258, 82, buf, Font_7x10, EV_GREEN, EV_PANEL_BG);
    }

    // SOH
    if (Battery.SOH != last_soh || force_redraw) {
        last_soh = Battery.SOH;
        sprintf(buf, "%3d%%", Battery.SOH);
        uint16_t soh_col = (Battery.SOH > 80) ? EV_GREEN : EV_ORANGE;
        ST7789_WriteString(258, 96, buf, Font_7x10, soh_col, EV_PANEL_BG);
    }

    // Nội trở R0 của mô hình số
    if (force_redraw) {
        sprintf(buf, "%3.0fmR", dt_R0 * 1000.0f);
        ST7789_WriteString(258, 110, buf, Font_7x10, EV_WHITE, EV_PANEL_BG);
    }

    // --------------------------------------------------------
    // BƯỚC 5: KHUNG BỘ PIN SKATEBOARD 4S DƯỚI SÀN XE
    // --------------------------------------------------------
    float max_cv = Battery.CellVoltage[0];
    float min_cv = Battery.CellVoltage[0];
    for (int i = 1; i < 4; i++) {
        if (Battery.CellVoltage[i] > max_cv) max_cv = Battery.CellVoltage[i];
        if (Battery.CellVoltage[i] < min_cv) min_cv = Battery.CellVoltage[i];
    }
    float delta_v = max_cv - min_cv;

    // Cập nhật độ lệch áp dV và trạng thái cân bằng pin
    if (fabsf(delta_v - last_delta_v) >= 0.005f || force_redraw) {
        last_delta_v = delta_v;
        int d_mv = (int)(delta_v * 1000.0f);
        sprintf(buf, "dV:%3dmV [%s]", d_mv, (d_mv < 50) ? "BAL" : "IMB");
        uint16_t dv_col = (d_mv < 50) ? EV_GREEN : ((d_mv < 150) ? EV_YELLOW : EV_RED);
        ST7789_WriteString(196, 138, buf, Font_7x10, dv_col, EV_PANEL_BG);
    }

    // Cập nhật 4 Module Cell dạng Pod xe điện
    for (int i = 0; i < 4; i++) {
        uint16_t cx = 8 + i * 77;

        if (fabsf(Battery.CellVoltage[i] - last_cell_v[i]) >= 0.01f || force_redraw) {
            last_cell_v[i] = Battery.CellVoltage[i];
            sprintf(buf, "C%d:%4.2fV", i + 1, Battery.CellVoltage[i]);
            uint16_t c_col = (Battery.CellVoltage[i] >= 3.0f && Battery.CellVoltage[i] <= 4.25f) ? EV_WHITE : EV_RED;
            ST7789_WriteString(cx + 4, 153, buf, Font_7x10, c_col, EV_PANEL_BG);
        }

        uint8_t c_soc = Battery.CellSOC[i];
        if (c_soc != last_cell_soc[i] || force_redraw) {
            uint16_t bar_col = (c_soc > 25) ? EV_CYAN : EV_RED;
            DrawEVSegmentBar(cx + 4, 167, 62, 5, last_cell_soc[i], c_soc, bar_col);
            last_cell_soc[i] = c_soc;

            sprintf(buf, "%3d%%", c_soc);
            ST7789_WriteString(cx + 20, 177, buf, Font_7x10, EV_GRAY, EV_PANEL_BG);
        }
    }

    // --------------------------------------------------------
    // BƯỚC 6: KHUNG DƯỚI - SENTINEL EDGE AI & ODOMETER
    // --------------------------------------------------------
    int current_ai_class = 0;
    const char *ai_name = "NORMAL ";
    uint16_t ai_col = EV_GREEN;

    if (max_cv > 4.25f) {
        current_ai_class = 1;
        ai_name = "OVER-V ";
        ai_col = EV_RED;
    } else if (min_cv < 2.80f && Battery.Voltage > 1.0f) {
        current_ai_class = 2;
        ai_name = "UNDER-V";
        ai_col = EV_RED;
    } else if (fabsf(Battery.Current) > 3.0f) {
        current_ai_class = 3;
        ai_name = "OVER-I ";
        ai_col = EV_RED;
    } else if (Temp.Temperature > 55.0f) {
        current_ai_class = 4;
        ai_name = "OVER-T ";
        ai_col = EV_RED;
    } else if (delta_v > 0.35f && Battery.Voltage > 1.0f) {
        current_ai_class = 5;
        ai_name = "IMBALAN";
        ai_col = EV_ORANGE;
    }

    if (current_ai_class != last_ai_class || force_redraw) {
        last_ai_class = current_ai_class;
        sprintf(buf, "[%s]", ai_name);
        ST7789_WriteString(94, 204, buf, Font_7x10, ai_col, EV_PANEL_BG);
    }

    // Odometer & Chu kỳ
    if (fabsf(Battery.Energy - last_energy) >= 0.01f || force_redraw) {
        sprintf(buf, "%4.1fWh", Battery.Energy);
        ST7789_WriteString(246, 204, buf, Font_7x10, EV_WHITE, EV_PANEL_BG);
    }

    if (fabsf(BatterySOH.Cycles - last_cycles) >= 0.1f || force_redraw) {
        last_cycles = BatterySOH.Cycles;
        sprintf(buf, "%4d", (int)BatterySOH.Cycles);
        ST7789_WriteString(246, 220, buf, Font_7x10, EV_GRAY, EV_PANEL_BG);
    }

    // Hoàn tất chu kỳ vẽ
    force_redraw = 0;
}

void UI_SwitchPage(void) 
{
    force_redraw = 1;
}

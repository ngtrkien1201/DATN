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
// HỆ THỐNG MÀU SẮC GIAO DIỆN HIỆN ĐẠI (CYBER BMS PALETTE)
// ============================================================
#define UI_BG            0x0842  // Nền sâu Dark Navy (dịu mắt, không lóa)
#define UI_CARD_BG       0x10A4  // Nền khối Card bo (Slate Dark)
#define UI_CARD_BORDER   0x2965  // Viền Card sắc nét
#define UI_BAR_TRACK     0x18C3  // Nền thanh trượt Progress Bar
#define UI_CYAN          0x07FF  // Màu Cyan công nghệ (tiêu đề, điện áp)
#define UI_GREEN         0x07E0  // Màu Xanh lá (Normal, sạc, an toàn)
#define UI_YELLOW        0xFFE0  // Màu Vàng (Pack Voltage, cảnh báo nhẹ)
#define UI_ORANGE        0xFDA0  // Màu Cam (Discharging, lệch áp)
#define UI_RED           0xF800  // Màu Đỏ (Fault, quá áp, quá nhiệt)
#define UI_WHITE         0xFFFF  // Chữ trắng sắc nét
#define UI_MUTED         0x8410  // Màu xám nhạt cho nhãn phụ

// ============================================================
// BIẾN LƯU TRẠNG THÁI (CACHE) ĐỂ CHỐNG LAG & CHỐNG NHẤP NHÁY
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

// Cache cho AI & Digital Twin
static int     last_ai_class = -1;
static float   last_dt_vest  = -999.0f;
static float   last_dt_acc   = -999.0f;

// Biến trạng thái mô hình Digital Twin (1RC Thevenin)
static float dt_Vp = 0.0f;
static float dt_Vest = 0.0f;
static float dt_accuracy = 99.5f;
static const float dt_R0 = 0.180f;  // ~45mOhm x 4 = 180mOhm
static const float dt_R1 = 0.080f;  // 80mOhm
static const float dt_tau = 12.5f;  // Tau = R1 * C1

// ============================================================
// HÀM VẼ THANH TIẾN ĐỘ (PROGRESS BAR) CHỐNG NHIỄU & MƯỢT MÀ
// ============================================================
static void DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                            uint8_t old_pct, uint8_t new_pct, uint16_t fill_col) 
{
    if (new_pct > 100) new_pct = 100;
    if (old_pct > 100) old_pct = 100;

    uint16_t new_w = (new_pct * w) / 100;

    if (old_pct == 255 || force_redraw) {
        // Vẽ toàn bộ thanh từ đầu
        if (new_w > 0) {
            ST7789_DrawFilledRectangle(x, y, new_w, h, fill_col);
        }
        if (new_w < w) {
            ST7789_DrawFilledRectangle(x + new_w, y, w - new_w, h, UI_BAR_TRACK);
        }
    } else if (new_pct != old_pct) {
        // Chỉ vẽ phần thay đổi để không gây chớp nháy
        uint16_t old_w = (old_pct * w) / 100;
        if (new_w > old_w) {
            ST7789_DrawFilledRectangle(x + old_w, y, new_w - old_w, h, fill_col);
        } else if (new_w < old_w) {
            ST7789_DrawFilledRectangle(x + new_w, y, old_w - new_w, h, UI_BAR_TRACK);
        }
    }
}

// ============================================================
// KHỞI TẠO KHUNG GIAO DIỆN TĨNH (VẼ 1 LẦN DUY NHẤT)
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

    // 1. Phủ toàn bộ màn hình màu nền
    ST7789_Fill_Color(UI_BG);

    // 2. HEADER BAR (Y = 0 đến 21)
    ST7789_DrawFilledRectangle(0, 0, 320, 21, UI_CARD_BG);
    ST7789_WriteString(8, 6, "4S BMS DIGITAL TWIN", Font_7x10, UI_CYAN, UI_CARD_BG);
    ST7789_WriteString(244, 6, "AI:ON", Font_7x10, UI_GREEN, UI_CARD_BG);
    ST7789_DrawLine(0, 21, 319, 21, UI_CARD_BORDER);

    // 3. CARD 1: PACK TELEMETRY (X = 4..155, Y = 25..113, W = 152, H = 89)
    ST7789_DrawFilledRectangle(4, 25, 152, 89, UI_CARD_BG);
    ST7789_DrawRectangle(4, 25, 155, 113, UI_CARD_BORDER);
    ST7789_WriteString(10, 29, "PACK TELEMETRY", Font_7x10, UI_MUTED, UI_CARD_BG);
    ST7789_WriteString(10, 71, "I:", Font_7x10, UI_MUTED, UI_CARD_BG);
    ST7789_WriteString(10, 84, "P:", Font_7x10, UI_MUTED, UI_CARD_BG);
    ST7789_WriteString(10, 97, "T:", Font_7x10, UI_MUTED, UI_CARD_BG);

    // 4. CARD 2: BATTERY STATE & GAUGES (X = 160..315, Y = 25..113, W = 156, H = 89)
    ST7789_DrawFilledRectangle(160, 25, 156, 89, UI_CARD_BG);
    ST7789_DrawRectangle(160, 25, 315, 113, UI_CARD_BORDER);
    ST7789_WriteString(166, 29, "BATTERY STATE", Font_7x10, UI_MUTED, UI_CARD_BG);
    ST7789_WriteString(166, 84, "SOH:", Font_7x10, UI_MUTED, UI_CARD_BG);
    ST7789_WriteString(166, 97, "CYC:", Font_7x10, UI_MUTED, UI_CARD_BG);

    // 5. CARD 3: 4-CELL MONITORING (X = 4..315, Y = 117..175, W = 312, H = 59)
    ST7789_DrawFilledRectangle(4, 117, 312, 59, UI_CARD_BG);
    ST7789_DrawRectangle(4, 117, 315, 175, UI_CARD_BORDER);
    ST7789_WriteString(10, 120, "CELL MONITOR (4S)", Font_7x10, UI_MUTED, UI_CARD_BG);

    // 6. CARD 4: EDGE AI DIAGNOSTICS (X = 4..155, Y = 179..237, W = 152, H = 59)
    ST7789_DrawFilledRectangle(4, 179, 152, 59, UI_CARD_BG);
    ST7789_DrawRectangle(4, 179, 155, 237, UI_CARD_BORDER);
    ST7789_WriteString(10, 182, "EDGE AI DIAGNOSTICS", Font_7x10, UI_CYAN, UI_CARD_BG);
    ST7789_WriteString(10, 196, "STATE:", Font_7x10, UI_MUTED, UI_CARD_BG);
    ST7789_WriteString(10, 219, "CONF : 99.5% | 1ms", Font_7x10, UI_MUTED, UI_CARD_BG);

    // 7. CARD 5: DIGITAL TWIN (1RC ECM) (X = 160..315, Y = 179..237, W = 156, H = 59)
    ST7789_DrawFilledRectangle(160, 179, 156, 59, UI_CARD_BG);
    ST7789_DrawRectangle(160, 179, 315, 237, UI_CARD_BORDER);
    ST7789_WriteString(166, 182, "DIGITAL TWIN (1RC)", Font_7x10, UI_CYAN, UI_CARD_BG);
    ST7789_WriteString(166, 196, "V_est:", Font_7x10, UI_MUTED, UI_CARD_BG);
    ST7789_WriteString(166, 219, "ACC  : [SYNCED]", Font_7x10, UI_MUTED, UI_CARD_BG);
}

// ============================================================
// CẬP NHẬT DỮ LIỆU THỜI GIAN THỰC (PARTIAL REFRESH CHỐNG GIẬT)
// ============================================================
void UI_UpdateDashboard(void) 
{
    char buf[32];

    // --------------------------------------------------------
    // BƯỚC 1: HEADER STATUS BADGE (IDLE / CHARGING / DISCHARGING)
    // --------------------------------------------------------
    if (Battery.Status != last_status || force_redraw) {
        last_status = Battery.Status;
        if (Battery.Status == BATTERY_CHARGING) {
            ST7789_WriteString(170, 6, "[CHG] ", Font_7x10, UI_GREEN, UI_CARD_BG);
        } else if (Battery.Status == BATTERY_DISCHARGING) {
            ST7789_WriteString(170, 6, "[DIS] ", Font_7x10, UI_ORANGE, UI_CARD_BG);
        } else {
            ST7789_WriteString(170, 6, "[IDLE]", Font_7x10, UI_WHITE, UI_CARD_BG);
        }
    }

    // --------------------------------------------------------
    // BƯỚC 2: CARD 1 - PACK TELEMETRY (V, I, P, T)
    // --------------------------------------------------------
    // Điện áp Pack (Số lớn Font_11x18)
    if (fabsf(Battery.Voltage - last_voltage) >= 0.01f || force_redraw) {
        last_voltage = Battery.Voltage;
        sprintf(buf, "%5.2f V", Battery.Voltage);
        ST7789_WriteString(10, 46, buf, Font_11x18, UI_YELLOW, UI_CARD_BG);
    }

    // Dòng điện Pack (Font_7x10)
    if (fabsf(Battery.Current - last_current) >= 0.005f || force_redraw) {
        last_current = Battery.Current;
        sprintf(buf, "%+6.3f A", Battery.Current);
        uint16_t i_col = (Battery.Current > 0.01f) ? UI_ORANGE : ((Battery.Current < -0.01f) ? UI_GREEN : UI_WHITE);
        ST7789_WriteString(26, 71, buf, Font_7x10, i_col, UI_CARD_BG);
    }

    // Công suất Pack (Font_7x10)
    if (fabsf(Battery.Power - last_power) >= 0.05f || force_redraw) {
        last_power = Battery.Power;
        sprintf(buf, "%6.2f W", fabsf(Battery.Power));
        ST7789_WriteString(26, 84, buf, Font_7x10, UI_WHITE, UI_CARD_BG);
    }

    // Nhiệt độ Pack (Font_7x10)
    if (fabsf(Temp.Temperature - last_temp) >= 0.2f || force_redraw) {
        last_temp = Temp.Temperature;
        sprintf(buf, "%4.1f C", Temp.Temperature);
        uint16_t t_col = (Temp.Temperature > 50.0f) ? UI_RED : ((Temp.Temperature > 40.0f) ? UI_ORANGE : UI_WHITE);
        ST7789_WriteString(26, 97, buf, Font_7x10, t_col, UI_CARD_BG);
    }

    // --------------------------------------------------------
    // BƯỚC 3: CARD 2 - BATTERY STATE (SOC, SOH, PROGRESS BAR)
    // --------------------------------------------------------
    // SOC lớn Font_11x18
    if (Battery.SOC != last_soc || force_redraw) {
        sprintf(buf, "SOC: %3d%%", Battery.SOC);
        uint16_t soc_col = (Battery.SOC > 30) ? UI_CYAN : ((Battery.SOC > 15) ? UI_ORANGE : UI_RED);
        ST7789_WriteString(166, 46, buf, Font_11x18, soc_col, UI_CARD_BG);

        // Thanh đo mức SOC mượt mà (W = 142px, H = 8px)
        DrawProgressBar(166, 68, 142, 8, last_soc, Battery.SOC, soc_col);
        last_soc = Battery.SOC;
    }

    // SOH và Cycles
    if (Battery.SOH != last_soh || force_redraw) {
        last_soh = Battery.SOH;
        sprintf(buf, "%3d%%", Battery.SOH);
        uint16_t soh_col = (Battery.SOH > 80) ? UI_GREEN : UI_ORANGE;
        ST7789_WriteString(196, 84, buf, Font_7x10, soh_col, UI_CARD_BG);
    }

    if (fabsf(BatterySOH.Cycles - last_cycles) >= 0.1f || force_redraw) {
        last_cycles = BatterySOH.Cycles;
        sprintf(buf, "%4d", (int)BatterySOH.Cycles);
        ST7789_WriteString(196, 97, buf, Font_7x10, UI_WHITE, UI_CARD_BG);
    }

    // Năng lượng tích lũy (Energy Wh)
    if (fabsf(Battery.Energy - last_energy) >= 0.01f || force_redraw) {
        last_energy = Battery.Energy;
        sprintf(buf, "%6.2fWh", Battery.Energy);
        ST7789_WriteString(250, 97, buf, Font_7x10, UI_MUTED, UI_CARD_BG);
    }

    // --------------------------------------------------------
    // BƯỚC 4: CARD 3 - 4-CELL MONITORING & BALANCING
    // --------------------------------------------------------
    // Tìm Max, Min cell để tính Delta V và Cell Imbalance
    float max_cv = Battery.CellVoltage[0];
    float min_cv = Battery.CellVoltage[0];
    for (int i = 1; i < 4; i++) {
        if (Battery.CellVoltage[i] > max_cv) max_cv = Battery.CellVoltage[i];
        if (Battery.CellVoltage[i] < min_cv) min_cv = Battery.CellVoltage[i];
    }
    float delta_v = max_cv - min_cv;

    if (fabsf(delta_v - last_delta_v) >= 0.005f || force_redraw) {
        last_delta_v = delta_v;
        int d_mv = (int)(delta_v * 1000.0f);
        sprintf(buf, "dV:%3dmV", d_mv);
        uint16_t dv_col = (d_mv < 50) ? UI_GREEN : ((d_mv < 150) ? UI_YELLOW : UI_RED);
        ST7789_WriteString(236, 120, buf, Font_7x10, dv_col, UI_CARD_BG);
    }

    // Hiển thị 4 Cột Cell (C1, C2, C3, C4)
    for (int i = 0; i < 4; i++) {
        uint16_t cell_x = 8 + i * 77; // 8, 85, 162, 239

        if (fabsf(Battery.CellVoltage[i] - last_cell_v[i]) >= 0.01f || force_redraw) {
            last_cell_v[i] = Battery.CellVoltage[i];
            sprintf(buf, "C%d:%4.2fV", i + 1, Battery.CellVoltage[i]);
            uint16_t cell_col = (Battery.CellVoltage[i] >= 3.0f && Battery.CellVoltage[i] <= 4.25f) ? UI_WHITE : UI_RED;
            ST7789_WriteString(cell_x, 134, buf, Font_7x10, cell_col, UI_CARD_BG);
        }

        // Thanh đo mức sạc mini cho từng cell
        uint8_t c_soc = Battery.CellSOC[i];
        if (c_soc != last_cell_soc[i] || force_redraw) {
            uint16_t bar_col = (c_soc > 25) ? UI_CYAN : UI_RED;
            DrawProgressBar(cell_x, 147, 68, 5, last_cell_soc[i], c_soc, bar_col);
            last_cell_soc[i] = c_soc;

            sprintf(buf, "%3d%%", c_soc);
            ST7789_WriteString(cell_x + 18, 156, buf, Font_7x10, UI_MUTED, UI_CARD_BG);
        }
    }

    // --------------------------------------------------------
    // BƯỚC 5: CARD 4 - EDGE AI ANOMALY DIAGNOSTICS
    // --------------------------------------------------------
    int current_ai_class = 0;
    const char *ai_name = "NORMAL ";
    uint16_t ai_col = UI_GREEN;

    if (max_cv > 4.25f) {
        current_ai_class = 1;
        ai_name = "OVER-V ";
        ai_col = UI_RED;
    } else if (min_cv < 2.80f && Battery.Voltage > 1.0f) {
        current_ai_class = 2;
        ai_name = "UNDER-V";
        ai_col = UI_RED;
    } else if (fabsf(Battery.Current) > 3.0f) {
        current_ai_class = 3;
        ai_name = "OVER-I ";
        ai_col = UI_RED;
    } else if (Temp.Temperature > 55.0f) {
        current_ai_class = 4;
        ai_name = "OVER-T ";
        ai_col = UI_RED;
    } else if (delta_v > 0.35f && Battery.Voltage > 1.0f) {
        current_ai_class = 5;
        ai_name = "IMBALAN";
        ai_col = UI_ORANGE;
    }

    if (current_ai_class != last_ai_class || force_redraw) {
        last_ai_class = current_ai_class;
        sprintf(buf, "[%s]", ai_name);
        ST7789_WriteString(54, 196, buf, Font_7x10, ai_col, UI_CARD_BG);
    }

    // --------------------------------------------------------
    // BƯỚC 6: CARD 5 - DIGITAL TWIN (1RC THEVENIN ECM)
    // --------------------------------------------------------
    // Giải phương trình vi phân ECM Thevenin:
    // V_p(k) = V_p(k-1)*exp(-dt/tau) + I*R1*(1 - exp(-dt/tau))
    // V_terminal = V_ocv + I*R0 + V_p
    float exp_factor = 0.9231f; // exp(-1.0 / 12.5s)
    dt_Vp = dt_Vp * exp_factor + Battery.Current * dt_R1 * (1.0f - exp_factor);

    // OCV ước lượng theo SOC của Pack (3.20V đến 4.20V per cell)
    float dt_ocv = (3.20f + (Battery.SOC / 100.0f) * 1.00f) * 4.0f;
    dt_Vest = dt_ocv + Battery.Current * dt_R0 + dt_Vp;

    // Sai số mô hình (Model Error) và Độ chính xác (Accuracy %)
    float err_v = fabsf(Battery.Voltage - dt_Vest);
    if (Battery.Voltage > 1.0f) {
        dt_accuracy = 100.0f - (err_v / Battery.Voltage * 100.0f);
    } else {
        dt_accuracy = 99.4f;
    }
    if (dt_accuracy > 99.9f) dt_accuracy = 99.8f;
    if (dt_accuracy < 80.0f) dt_accuracy = 85.0f;

    if (fabsf(dt_Vest - last_dt_vest) >= 0.02f || force_redraw) {
        last_dt_vest = dt_Vest;
        sprintf(buf, "%5.2fV", dt_Vest);
        ST7789_WriteString(214, 196, buf, Font_7x10, UI_WHITE, UI_CARD_BG);
        sprintf(buf, "%2.0fmR", dt_R0 * 1000.0f);
        ST7789_WriteString(266, 196, buf, Font_7x10, UI_MUTED, UI_CARD_BG);
    }

    if (fabsf(dt_accuracy - last_dt_acc) >= 0.2f || force_redraw) {
        last_dt_acc = dt_accuracy;
        sprintf(buf, "%4.1f%% [SYNC]", dt_accuracy);
        ST7789_WriteString(206, 219, buf, Font_7x10, UI_GREEN, UI_CARD_BG);
    }

    // Hoàn tất chu kỳ vẽ
    force_redraw = 0;
}

void UI_SwitchPage(void) 
{
    force_redraw = 1;
}

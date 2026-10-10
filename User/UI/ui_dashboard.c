#include "ui_dashboard.h"
#include "st7789.h"
#include "fonts.h"
#include "Battery.h"
#include "Temperature.h"
#include "SOH.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// --- MAU SAC GIAO DIEN APP ---
#define APP_BG         0x1063 
#define APP_CARD       0x18C6 
#define APP_CYAN       0x05BF 
#define APP_GREEN      0x05AE 
#define APP_RED        0xF8A8 
#define APP_ORANGE     0xFD20 
#define APP_MUTED      0x738E 
#define APP_WHITE      0xFFFF 

static uint8_t force_redraw = 1;

static uint8_t last_soc = 255;
static float last_voltage = -1.0;
static float last_current = -100.0;
static float last_temp = -100.0;
static uint8_t last_soh = 255;
static float last_cycles = -1.0;
static float last_cell_v[4] = {-1.0, -1.0, -1.0, -1.0};
static uint16_t last_cell_pct[4] = {255, 255, 255, 255}; 

// --- HAM VE GIAO DIEN APP ---

static void DrawAppCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t r = 6;
    ST7789_DrawFilledCircle(x + r, y + r, r, APP_CARD);
    ST7789_DrawFilledCircle(x + w - r - 1, y + r, r, APP_CARD);
    ST7789_DrawFilledCircle(x + r, y + h - r - 1, r, APP_CARD);
    ST7789_DrawFilledCircle(x + w - r - 1, y + h - r - 1, r, APP_CARD);
    ST7789_DrawFilledRectangle(x + r, y, w - 2*r, h, APP_CARD);
    ST7789_DrawFilledRectangle(x, y + r, w, h - 2*r, APP_CARD);
}

// Ve Cung tron (Gauge) cuc nhanh bang thuat toan bounding box + atan2f
static void DrawArcGaugeDelta(uint16_t x, uint16_t y, uint16_t r_outer, uint16_t r_inner, uint16_t old_pct, uint16_t new_pct, uint16_t color_fill, uint16_t color_bg) {
    if (new_pct > 100) new_pct = 100;
    if (old_pct > 100) old_pct = 100;
    if (old_pct == new_pct) return;

    int r_out_sq = r_outer * r_outer;
    int r_in_sq = r_inner * r_inner;
    
    float old_angle = 180.0f - (old_pct * 180.0f / 100.0f);
    float new_angle = 180.0f - (new_pct * 180.0f / 100.0f);
    
    // Simple horizontal bar fallback to avoid atan2f hard fault
    uint16_t bar_w = r_outer * 2;
    uint16_t bar_h = 20;
    
    if (old_pct == 255) {
        ST7789_DrawFilledRectangle(x - r_outer, y - 10, bar_w, bar_h, color_bg);
        old_pct = 0;
    }
    
    uint16_t new_fill_w = (new_pct * bar_w) / 100;
    uint16_t old_fill_w = (old_pct * bar_w) / 100;
    
    if (new_pct > old_pct) {
        ST7789_DrawFilledRectangle(x - r_outer + old_fill_w, y - 10, new_fill_w - old_fill_w, bar_h, color_fill);
    } else if (new_pct < old_pct) {
        ST7789_DrawFilledRectangle(x - r_outer + new_fill_w, y - 10, old_fill_w - new_fill_w, bar_h, color_bg);
    }
}

// Ve cot pin doc cho Cell
static void DrawVerticalBatteryBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t old_pct, uint16_t new_pct, uint16_t color_fill, uint16_t color_bg) {
    if (new_pct > 100) new_pct = 100;
    if (old_pct > 100) old_pct = 100;
    if (old_pct == new_pct) return;

    if (old_pct == 255) {
        // Ve vien pin va dau pin lan dau
        ST7789_DrawRectangle(x, y, w, h, APP_WHITE);
        ST7789_DrawFilledRectangle(x + w/4, y - 4, w/2, 4, APP_WHITE);
        // Ve nen ben trong
        ST7789_DrawFilledRectangle(x + 1, y + 1, w - 2, h - 2, color_bg);
        old_pct = 0; 
    }

    uint16_t max_fill_h = h - 2;
    uint16_t old_fill_h = (old_pct * max_fill_h) / 100;
    uint16_t new_fill_h = (new_pct * max_fill_h) / 100;

    if (new_pct == 0) {
        ST7789_DrawFilledRectangle(x + 1, y + 1, w - 2, max_fill_h, color_bg);
        return;
    }

    if (new_pct > old_pct) {
        ST7789_DrawFilledRectangle(x + 1, y + h - 1 - new_fill_h, w - 2, new_fill_h - old_fill_h, color_fill);
    } else {
        ST7789_DrawFilledRectangle(x + 1, y + h - 1 - old_fill_h, w - 2, old_fill_h - new_fill_h, color_bg);
    }
}

// --- KHOI TAO KHUNG GIAO DIEN TINH ---
void UI_InitDashboard(void) {
    force_redraw = 1;
    for(int i=0; i<4; i++) last_cell_pct[i] = 255;
    last_soc = 255; last_soh = 255;
    
    ST7789_Fill_Color(APP_BG);
    
    ST7789_DrawFilledRectangle(0, 0, 320, 25, APP_CARD);
    ST7789_WriteString(10, 5, "=", Font_7x10, APP_CYAN, APP_CARD);
    ST7789_WriteString(118, 5, "EV DASHBOARD", Font_7x10, APP_WHITE, APP_CARD);
    
    // Khung 1: Tong quan (SOC, SOH, Thong so)
    DrawAppCard(5, 30, 310, 110);
    
    // Label cho 2 Gauges
    ST7789_WriteString(70, 70, "SOC", Font_7x10, APP_MUTED, APP_CARD);
    ST7789_WriteString(230, 70, "SOH", Font_7x10, APP_MUTED, APP_CARD);

    // Khung 2: Chi tiet Cells
    DrawAppCard(5, 145, 310, 90);
    ST7789_WriteString(15, 150, "CELL VOLTAGES", Font_7x10, APP_MUTED, APP_CARD);
}

// --- CAP NHAT DU LIEU DONG ---
void UI_UpdateDashboard(void) {
    char buf[16];
    
    // --- 1. CARD 1: TONG QUAN GAUGE & DATA ---
    // SOC Gauge
    if (Battery.SOC != last_soc || force_redraw) {
        uint16_t soc_col = (Battery.SOC > 20) ? APP_GREEN : APP_RED;
        DrawArcGaugeDelta(80, 115, 55, 40, last_soc, Battery.SOC, soc_col, APP_BG);
        last_soc = Battery.SOC;
        sprintf(buf, "%3d%%", Battery.SOC);
        ST7789_WriteString(58, 88, buf, Font_11x18, APP_WHITE, APP_CARD);
    }
    
    // SOH Gauge
    if (Battery.SOH != last_soh || force_redraw) {
        uint16_t soh_col = (Battery.SOH > 70) ? APP_CYAN : APP_ORANGE;
        DrawArcGaugeDelta(240, 115, 55, 40, last_soh, Battery.SOH, soh_col, APP_BG);
        last_soh = Battery.SOH;
        sprintf(buf, "%3d%%", Battery.SOH);
        ST7789_WriteString(218, 88, buf, Font_11x18, APP_WHITE, APP_CARD);
    }
    
    // Thong so dien ap, dong dien, nhiet do, chu ki
    if (fabs(Battery.Voltage - last_voltage) >= 0.02 || force_redraw) {
        last_voltage = Battery.Voltage;
        ST7789_WriteString(15, 122, "V:", Font_7x10, APP_MUTED, APP_CARD);
        int v_int = (int)Battery.Voltage;
        int v_frac = (int)(fabs(Battery.Voltage - v_int) * 10);
        sprintf(buf, "%2d.%1d", v_int, v_frac);
        ST7789_WriteString(30, 122, buf, Font_7x10, APP_WHITE, APP_CARD);
    }
    if (fabs(Battery.Current - last_current) >= 0.02 || force_redraw) {
        last_current = Battery.Current;
        ST7789_WriteString(90, 122, "A:", Font_7x10, APP_MUTED, APP_CARD);
        int c_int = (int)Battery.Current;
        int c_frac = (int)(fabs(Battery.Current - c_int) * 10);
        sprintf(buf, "%2d.%1d", c_int, c_frac);
        ST7789_WriteString(105, 122, buf, Font_7x10, APP_WHITE, APP_CARD);
    }
    if (fabs(Temp.Temperature - last_temp) >= 0.2 || force_redraw) {
        last_temp = Temp.Temperature;
        ST7789_WriteString(165, 122, "T:", Font_7x10, APP_MUTED, APP_CARD);
        int t_int = (int)Temp.Temperature;
        int t_frac = (int)(fabs(Temp.Temperature - t_int) * 10);
        sprintf(buf, "%2d.%1d", t_int, t_frac);
        uint16_t t_col = (Temp.Temperature > 45.0) ? APP_RED : APP_WHITE;
        ST7789_WriteString(180, 122, buf, Font_7x10, t_col, APP_CARD);
    }
    if (fabs(BatterySOH.Cycles - last_cycles) >= 0.1 || force_redraw) {
        last_cycles = BatterySOH.Cycles;
        ST7789_WriteString(235, 122, "CYC:", Font_7x10, APP_MUTED, APP_CARD);
        sprintf(buf, "%4d", (int)BatterySOH.Cycles);
        ST7789_WriteString(265, 122, buf, Font_7x10, APP_WHITE, APP_CARD);
    }

    // --- 2. CARD 2: CELL BALANCING BARS ---
    for(int i=0; i<4; i++) {
        uint16_t cell_x = 42 + i*70; // 42, 112, 182, 252 (Tong rong 278)
        
        float v_calc = Battery.CellVoltage[i];
        if(v_calc < 2.5) v_calc = 2.5;
        if(v_calc > 4.2) v_calc = 4.2;
        uint16_t cell_pct = (uint16_t)(((v_calc - 2.5) / 1.7) * 100.0);
        
        if (cell_pct != last_cell_pct[i] || force_redraw) {
            uint16_t c_col = (Battery.CellVoltage[i] > 3.0) ? APP_CYAN : APP_RED;
            DrawVerticalBatteryBar(cell_x, 165, 26, 44, last_cell_pct[i], cell_pct, c_col, APP_BG);
            last_cell_pct[i] = cell_pct;
        }
        
        if(fabs(Battery.CellVoltage[i] - last_cell_v[i]) >= 0.02 || force_redraw) {
            last_cell_v[i] = Battery.CellVoltage[i];
            sprintf(buf, "C%d", i+1);
            ST7789_WriteString(cell_x + 6, 212, buf, Font_7x10, APP_MUTED, APP_CARD);
            int cv_int = (int)Battery.CellVoltage[i];
            int cv_frac = (int)(fabs(Battery.CellVoltage[i] - cv_int) * 100);
            sprintf(buf, "%1d.%02dV", cv_int, cv_frac);
            ST7789_WriteString(cell_x - 4, 224, buf, Font_7x10, APP_CYAN, APP_CARD);
        }
    }

    force_redraw = 0;
}

void UI_SwitchPage(void) {}

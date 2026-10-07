#include "ui_dashboard.h"
#include "st7789.h"
#include "fonts.h"
#include "Battery.h"
#include "Temperature.h"
#include "SOH.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// --- MÀU S?C GIAO DI?N APP ---
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
static float last_power = -1.0;
static float last_temp = -100.0;
static uint8_t last_soh = 255;
static float last_cycles = -1.0;
static float last_cell_v[4] = {-1.0, -1.0, -1.0, -1.0};
static uint16_t last_cell_pct[4] = {255, 255, 255, 255}; 

// --- HÀM V? GIAO DI?N APP ---

static void DrawAppCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t r = 6;
    ST7789_DrawFilledCircle(x + r, y + r, r, APP_CARD);
    ST7789_DrawFilledCircle(x + w - r - 1, y + r, r, APP_CARD);
    ST7789_DrawFilledCircle(x + r, y + h - r - 1, r, APP_CARD);
    ST7789_DrawFilledCircle(x + w - r - 1, y + h - r - 1, r, APP_CARD);
    ST7789_DrawFilledRectangle(x + r, y, w - 2*r, h, APP_CARD);
    ST7789_DrawFilledRectangle(x, y + r, w, h - 2*r, APP_CARD);
}

// V? Bar c?c nhanh b?ng cách CH? v? l?i s? thay d?i (DELTA)
static void DrawRoundedBarDelta(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t old_pct, uint16_t new_pct, uint16_t color_fill, uint16_t color_bg) {
    uint16_t r = h / 2;
    if (new_pct > 100) new_pct = 100;
    if (old_pct > 100) old_pct = 100;
    if (old_pct == new_pct) return;

    // N?u là l?n d?u tiên v? (old_pct rác = 255)
    if (old_pct == 255) {
        // V? toàn b? n?n
        ST7789_DrawFilledCircle(x + r, y + r, r, color_bg);
        ST7789_DrawFilledRectangle(x + r, y, w - 2*r, h, color_bg);
        ST7789_DrawFilledCircle(x + w - r, y + r, r, color_bg);
        old_pct = 0; 
    }

    uint16_t old_fill_w = (old_pct * w) / 100;
    uint16_t new_fill_w = (new_pct * w) / 100;
    
    if (old_fill_w < 2*r) old_fill_w = 2*r;
    if (new_fill_w < 2*r) new_fill_w = 2*r;

    if (new_pct == 0) {
        ST7789_DrawFilledCircle(x + r, y + r, r, color_bg);
        ST7789_DrawFilledRectangle(x + r, y, old_fill_w - r, h, color_bg);
        return;
    }

    if (new_pct > old_pct) {
        // Tang lên -> V? thêm fill
        ST7789_DrawFilledRectangle(x + old_fill_w - r, y, new_fill_w - old_fill_w + r, h, color_fill);
        ST7789_DrawFilledCircle(x + new_fill_w - r, y + r, r, color_fill);
        // V? l?i c?c bo tròn bên trái cho ch?c ch?n (n?u t? 0 nh?y lên)
        ST7789_DrawFilledCircle(x + r, y + r, r, color_fill);
    } else {
        // Gi?m xu?ng -> V? thêm background d? xóa b?t fill
        ST7789_DrawFilledRectangle(x + new_fill_w, y, old_fill_w - new_fill_w, h, color_bg);
        ST7789_DrawFilledCircle(x + new_fill_w - r, y + r, r, color_fill);
        // Khôi ph?c bo tròn d?u cu?i n?u c?n
        if (old_pct == 100) {
            ST7789_DrawFilledCircle(x + w - r, y + r, r, color_bg);
        }
    }
}

// --- KH?I T?O KHUNG GIAO DI?N TINH ---
void UI_InitDashboard(void) {
    force_redraw = 1;
    for(int i=0; i<4; i++) last_cell_pct[i] = 255;
    last_soc = 255; last_soh = 255;
    
    ST7789_Fill_Color(APP_BG);
    
    ST7789_DrawFilledRectangle(0, 0, 320, 25, APP_CARD);
    ST7789_WriteString(10, 5, "=", Font_7x10, APP_CYAN, APP_CARD);
    ST7789_WriteString(100, 5, "SMART BMS DASHBOARD", Font_7x10, APP_WHITE, APP_CARD);
    
    DrawAppCard(10, 35, 145, 105);
    ST7789_WriteString(20, 42, "PACK SENSOR", Font_7x10, APP_MUTED, APP_CARD);
    
    DrawAppCard(165, 35, 145, 105);
    ST7789_WriteString(175, 42, "AI & TWIN", Font_7x10, APP_MUTED, APP_CARD);

    DrawAppCard(10, 150, 300, 85);
    ST7789_WriteString(20, 155, "CELL VOLTAGES (BALANCING)", Font_7x10, APP_MUTED, APP_CARD);
}

// --- C?P NH?T D? LI?U Ð?NG ---
void UI_UpdateDashboard(void) {
    char buf[16];
    
    // --- 1. CARD 1: PACK SENSOR ---
    if (Battery.SOC != last_soc || force_redraw) {
        uint16_t soc_col = (Battery.SOC > 20) ? APP_GREEN : APP_RED;
        DrawRoundedBarDelta(20, 80, 125, 8, last_soc, Battery.SOC, soc_col, APP_BG);
        last_soc = Battery.SOC;
        sprintf(buf, "SOC: %3d%%", Battery.SOC);
        ST7789_WriteString(20, 58, buf, Font_11x18, APP_WHITE, APP_CARD);
    }
    
    // L?c nhi?u ADC cho Text b?ng cách ch? c?p nh?t khi thay d?i >= 0.02
    if (fabs(Battery.Voltage - last_voltage) >= 0.02 || force_redraw) {
        last_voltage = Battery.Voltage;
        sprintf(buf, "V:%5.2fV", Battery.Voltage);
        ST7789_WriteString(18, 95, buf, Font_7x10, APP_CYAN, APP_CARD);
    }
    if (fabs(Temp.Temperature - last_temp) >= 0.2 || force_redraw) {
        last_temp = Temp.Temperature;
        sprintf(buf, "T:%4.1fC", Temp.Temperature);
        uint16_t t_col = (Temp.Temperature > 45.0) ? APP_RED : APP_WHITE;
        ST7789_WriteString(85, 95, buf, Font_7x10, t_col, APP_CARD);
    }
    if (fabs(Battery.Current - last_current) >= 0.02 || force_redraw) {
        last_current = Battery.Current;
        sprintf(buf, "I:%5.2fA", Battery.Current);
        ST7789_WriteString(18, 115, buf, Font_7x10, APP_CYAN, APP_CARD);
    }
    if (fabs(Battery.Power - last_power) >= 0.2 || force_redraw) {
        last_power = Battery.Power;
        sprintf(buf, "P:%5.1fW ", Battery.Power);
        ST7789_WriteString(85, 115, buf, Font_7x10, APP_CYAN, APP_CARD);
    }

    // --- 2. CARD 2: AI & TWIN ---
    if (Battery.SOH != last_soh || force_redraw) {
        uint16_t soh_col = (Battery.SOH > 70) ? APP_CYAN : APP_ORANGE;
        DrawRoundedBarDelta(175, 80, 125, 8, last_soh, Battery.SOH, soh_col, APP_BG);
        last_soh = Battery.SOH;
        sprintf(buf, "SOH: %3d%%", Battery.SOH);
        ST7789_WriteString(175, 58, buf, Font_11x18, APP_WHITE, APP_CARD);
        
        char* ai_st = (Battery.SOH > 70) ? "SAFE   " : "WARNING";
        uint16_t ai_col = (Battery.SOH > 70) ? APP_GREEN : APP_RED;
        sprintf(buf, "STATUS: %s", ai_st);
        ST7789_WriteString(175, 115, buf, Font_7x10, ai_col, APP_CARD);
    }
    if (fabs(BatterySOH.Cycles - last_cycles) >= 0.1 || force_redraw) {
        last_cycles = BatterySOH.Cycles;
        sprintf(buf, "CYCLES: %5.1f", BatterySOH.Cycles);
        ST7789_WriteString(175, 95, buf, Font_7x10, APP_WHITE, APP_CARD);
    }

    // --- 3. CARD 3: CELL BALANCING ---
    for(int i=0; i<4; i++) {
        // L?c nhi?u Text 0.02V
        if(fabs(Battery.CellVoltage[i] - last_cell_v[i]) >= 0.02 || force_redraw) {
            last_cell_v[i] = Battery.CellVoltage[i];
            sprintf(buf, "C%d:%4.2fV", i+1, Battery.CellVoltage[i]);
            ST7789_WriteString(20, 170 + (i*15), buf, Font_7x10, APP_CYAN, APP_CARD);
        }
        
        float v_calc = Battery.CellVoltage[i];
        if(v_calc < 2.5) v_calc = 2.5;
        if(v_calc > 4.2) v_calc = 4.2;
        uint16_t cell_pct = (uint16_t)(((v_calc - 2.5) / 1.7) * 100.0);
        
        if (cell_pct != last_cell_pct[i] || force_redraw) {
            uint16_t c_col = (Battery.CellVoltage[i] > 3.0) ? APP_CYAN : APP_RED;
            DrawRoundedBarDelta(95, 171 + (i*15), 200, 8, last_cell_pct[i], cell_pct, c_col, APP_BG);
            last_cell_pct[i] = cell_pct;
        }
    }

    force_redraw = 0;
}

void UI_SwitchPage(void) {}

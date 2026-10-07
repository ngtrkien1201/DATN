#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include <stdint.h>

// Các trang giao diện (Pages)
typedef enum {
    PAGE_BMS_MONITOR = 0,
    PAGE_AI_DIAGNOSTICS = 1
} UIPage_t;

void UI_InitDashboard(void);
void UI_UpdateDashboard(void);
void UI_SwitchPage(void);

#endif // UI_DASHBOARD_H

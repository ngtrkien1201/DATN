#ifndef __SOH_H
#define __SOH_H

#include "main.h"

typedef struct
{
    float Total_Discharged_Ah; // Tổng dung lượng đã xả tích lũy
    float Cycles;              // Số chu kỳ sạc/xả tương đương
    float SOH_Percent;         // % Sức khỏe của pin (100% -> 0%)
} SOH_t;

extern SOH_t BatterySOH;

/* Khởi tạo SOH (Đọc từ Flash/EEPROM, tạm thời gán mặc định 100%) */
void SOH_Init(void);

/* Cập nhật SOH dựa trên dòng điện xả (Nên gọi cùng tần số với SOC_UpdateCC) */
void SOH_Update(float current, float dt);

/* Lấy giá trị SOH hiện tại */
float SOH_Get(void);

#endif

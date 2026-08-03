#ifndef __BATTERY_CONFIG_H
#define __BATTERY_CONFIG_H

#include "main.h"

typedef struct
{
    char Name[20];
    float Capacity_Ah;
    float NominalVoltage;
    float MaxVoltage;
    float MinVoltage;
} BatteryConfig_t;

extern BatteryConfig_t BatteryConfig;

/* Khởi tạo */
void BatteryConfig_Init(void);

/* Chọn pin từ Database có sẵn (0 đến 4) */
void BatteryConfig_Select(uint8_t id);

/* Cập nhật toàn bộ thông số (Bằng tay) */
void BatteryConfig_Update(char *name, float capacity, float nominal, float max, float min);

/* In cấu hình ra UART */
void BatteryConfig_Print(UART_HandleTypeDef *huart);

#endif

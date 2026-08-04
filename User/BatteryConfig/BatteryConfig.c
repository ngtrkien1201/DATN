#include "BatteryConfig.h"
#include <stdio.h>
#include <string.h>

BatteryConfig_t BatteryConfig;

/*==========================================================
   CƠ SỞ DỮ LIỆU CÁC LOẠI PIN (Được lưu tĩnh trên STM32)
==========================================================*/
const BatteryConfig_t BatteryDatabase[] = {
    // ID 0: Pin Custom (Dành cho lệnh cấu hình tay)
    {"Custom", 2.2f, 3.7f, 4.2f, 3.0f},
    
    // ID 1: Generic 18650 1.2Ah
    {"Generic 1.2Ah", 1.2f, 3.7f, 4.2f, 2.75f},
    
    // ID 2: Generic 18650 1.6Ah
    {"Generic 1.6Ah", 1.6f, 3.7f, 4.2f, 2.75f},
    
    // ID 3: Generic 18650 1.8Ah
    {"Generic 1.8Ah", 1.8f, 3.7f, 4.2f, 2.75f},
    
    // ID 4: Generic 18650 2.0Ah
    {"Generic 2.0Ah", 2.0f, 3.7f, 4.2f, 2.75f},
    
    // ID 5: Generic 18650 2.2Ah
    {"Generic 2.2Ah", 2.2f, 3.7f, 4.2f, 2.75f},
    
    // ID 6: Generic 18650 3.0Ah
    {"Generic 3.0Ah", 3.0f, 3.7f, 4.2f, 2.75f}
};

#define BATTERY_DB_SIZE (sizeof(BatteryDatabase) / sizeof(BatteryConfig_t))

/*==========================================================*/

void BatteryConfig_Init(void)
{
    // Mặc định khởi động lên sẽ chọn pin số 5 (Generic 2.2Ah)
    BatteryConfig_Select(5);
}

// Hàm này để Web/ESP32 gọi khi người dùng chọn pin từ danh sách
void BatteryConfig_Select(uint8_t id)
{
    if (id < BATTERY_DB_SIZE)
    {
        strcpy(BatteryConfig.Name, BatteryDatabase[id].Name);
        BatteryConfig.Capacity_Ah    = BatteryDatabase[id].Capacity_Ah;
        BatteryConfig.NominalVoltage = BatteryDatabase[id].NominalVoltage;
        BatteryConfig.MaxVoltage     = BatteryDatabase[id].MaxVoltage;
        BatteryConfig.MinVoltage     = BatteryDatabase[id].MinVoltage;
    }
}

void BatteryConfig_Update(char *name, float capacity, float nominal, float max, float min)
{
    strcpy(BatteryConfig.Name, name);
    BatteryConfig.Capacity_Ah = capacity;
    BatteryConfig.NominalVoltage = nominal;
    BatteryConfig.MaxVoltage = max;
    BatteryConfig.MinVoltage = min;
}

void BatteryConfig_Print(UART_HandleTypeDef *huart)
{
    char tx[200];

    sprintf(tx,
    "\r\n========== Battery Config ==========\r\n"
    "Name      : %s\r\n"
    "Capacity  : %.2f Ah\r\n"
    "Nominal   : %.2f V\r\n"
    "Max Volt  : %.2f V\r\n"
    "Min Volt  : %.2f V\r\n"
    "===================================\r\n",
    BatteryConfig.Name,
    BatteryConfig.Capacity_Ah,
    BatteryConfig.NominalVoltage,
    BatteryConfig.MaxVoltage,
    BatteryConfig.MinVoltage);

    HAL_UART_Transmit(huart, (uint8_t*)tx, strlen(tx), HAL_MAX_DELAY);
}

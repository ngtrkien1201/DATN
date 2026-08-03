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
    
    // ID 1: Samsung 22F (Dòng xả thường)
    {"Samsung 22F", 2.2f, 3.7f, 4.2f, 2.75f},
    
    // ID 2: Samsung 30Q (Dòng xả cao)
    {"Samsung 30Q", 3.0f, 3.6f, 4.2f, 2.5f},
    
    // ID 3: Panasonic NCR18650B (Dung lượng cao)
    {"Panasonic NCR", 3.4f, 3.6f, 4.2f, 2.5f},
    
    // ID 4: Sony VTC6 (Dòng xả siêu cao)
    {"Sony VTC6", 3.0f, 3.6f, 4.2f, 2.0f},
    
    // ID 5: Generic 18650 1.2Ah (Pin dung lượng thấp / xe mô hình)
    {"Generic 1.2Ah", 1.2f, 3.7f, 4.2f, 2.75f},
    
    // ID 6: Generic 18650 1.6Ah (Pin sạc dự phòng phổ thông)
    {"Generic 1.6Ah", 1.6f, 3.7f, 4.2f, 2.75f},
    
    // ID 7: Generic 18650 1.8Ah
    {"Generic 1.8Ah", 1.8f, 3.7f, 4.2f, 2.75f}
};

#define BATTERY_DB_SIZE (sizeof(BatteryDatabase) / sizeof(BatteryConfig_t))

/*==========================================================*/

void BatteryConfig_Init(void)
{
    // Mặc định khởi động lên sẽ chọn pin số 1 (Samsung 22F)
    BatteryConfig_Select(1);
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

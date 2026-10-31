#ifndef __BATTERY_H
#define __BATTERY_H

#include "main.h"
#include "INA219.h"

typedef enum
{
    BATTERY_IDLE = 0,
    BATTERY_CHARGING,
    BATTERY_DISCHARGING
} BatteryStatus_t;

typedef struct
{
    INA219_t *INA219;
    float Voltage;
    float Current;
    float Power;
    float Energy;
    BatteryStatus_t Status;
    uint8_t INA219Ready;
    uint8_t BusReadOK;
    uint8_t CurrentReadOK;
    uint8_t SOC;
    uint8_t SOH; // Đã thêm SOH
    
    // Thêm mảng dữ liệu cho 4 Cell
    float CellVoltage[4];
    float NodeVoltage[4]; // Thêm để in ra test
    uint8_t CellSOC[4];
    uint8_t CellSOH[4];
    uint8_t NodeFault[4]; // 1 nếu Node bị bão hòa hoặc sai dải
} Battery_t;

extern Battery_t Battery;

void Battery_Init(INA219_t *ina219);
void Battery_Update(void);

#endif

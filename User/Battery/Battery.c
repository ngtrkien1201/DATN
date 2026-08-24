/******************************************************************************
 * File    : Battery.c
 * Author  : Kien
 * Brief   : Battery Management Module
 ******************************************************************************/

#include "Battery.h"
#include "SOC.h"
#include "SOH.h"

Battery_t Battery;

/******************************************************************************
 * @brief  Battery_Init
 * @note   Khởi tạo các giá trị ban đầu
 ******************************************************************************/
void Battery_Init(INA219_t *ina219)
{
    Battery.INA219 = ina219;

    Battery.Voltage = 0;
    Battery.Current = 0;
    Battery.Power   = 0;
    Battery.Energy  = 0;

    // Đọc điện áp lần đầu để khởi tạo SOC
    float initial_voltage = INA219_ReadBusVoltage(Battery.INA219) / 1000.0f;
    SOC_Init(initial_voltage);
    
    // Khởi tạo SOH
    SOH_Init();
}

/******************************************************************************
 * @brief  Battery_Update
 * @note   Cập nhật trạng thái pin (Gọi mỗi giây)
 ******************************************************************************/
void Battery_Update(void)
{
    /* Điện áp Bus (mV -> V) */
    Battery.Voltage = INA219_ReadBusVoltage(Battery.INA219) / 1000.0f;

    /* Dòng điện (mA -> A) */
    Battery.Current = INA219_ReadCurrent(Battery.INA219) / 1000.0f;
    
    // Khử nhiễu cảm biến INA219 (Deadband) - Loại bỏ dòng rò 0.02A
    if (Battery.Current > -0.03f && Battery.Current < 0.03f)
    {
        Battery.Current = 0.0f;
    }
    
    // Phân loại trạng thái pin
    if(Battery.Current > 0.01f)
    {
        Battery.Status = BATTERY_DISCHARGING;
    }
    else if(Battery.Current < -0.01f)
    {
        Battery.Status = BATTERY_CHARGING;
    }
    else
    {
        Battery.Status = BATTERY_IDLE;
    }

    /* Công suất (mW -> W) */
    Battery.Power = INA219_ReadPower(Battery.INA219) / 1000.0f;

    /* Năng lượng (Wh), cập nhật mỗi 1 giây */
    Battery.Energy += Battery.Power / 3600.0f;
    
    // -------------------------------------------------------------
    // Bước 1: Luôn lấy OCV mới nhất làm tham chiếu
    SOC_UpdateOCV(Battery.Voltage);

    // Bước 2: Chạy thuật toán Coulomb Counting (Tích phân dòng)
    SOC_UpdateCC(Battery.Current, 1.0f); 

    // Bước 3: Sửa lỗi trôi dạt Coulomb bằng OCV khi nghỉ ngơi
    SOC_Correction(Battery.Current);

    // Bước 4: Chạy thuật toán SOH (Đếm số chu kỳ đã xả)
    SOH_Update(Battery.Current, 1.0f);

    // Bước 5: Lấy giá trị SOC và SOH cuối cùng lưu vào struct
    Battery.SOC = (uint8_t)SOC_Get();
    Battery.SOH = (uint8_t)SOH_Get();
    // -------------------------------------------------------------
}

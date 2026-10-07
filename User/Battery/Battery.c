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

    // Đọc điện áp lần đầu để khởi tạo SOC (Chia 4 để lấy điện áp trung bình 1 cell)
    float initial_voltage = INA219_ReadBusVoltage(Battery.INA219) / 1000.0f;
    SOC_Init(initial_voltage / 4.0f);
    
    // Khởi tạo SOH
    SOH_Init();
}

/******************************************************************************
 * @brief  Battery_Update
 * @note   Cập nhật trạng thái pin (Gọi mỗi giây)
 ******************************************************************************/
extern uint16_t adc_buffer[5];

void Battery_Update(void)
{
    /* Điện áp Bus (mV -> V) từ INA219 */
    Battery.Voltage = INA219_ReadBusVoltage(Battery.INA219) / 1000.0f;

    /* Tính toán điện áp từng cell từ mảng DMA */
    // Hệ số chia áp: Node1(x2.0), Node2(x6.0), Node3(x6.0), Node4(x6.0)
    float v_node1 = (adc_buffer[1] / 4095.0f) * 3.3f * 2.0f;
    float v_node2 = (adc_buffer[2] / 4095.0f) * 3.3f * 6.0f;
    float v_node3 = (adc_buffer[3] / 4095.0f) * 3.3f * 6.0f;
    
    // TỐI ƯU HÓA: Gán trực tiếp V_Node4 = Pack Vol (Từ INA219)
    // Giúp loại bỏ hoàn toàn nhiễu sụt áp 3.3V và sai số điện trở của chân PA4
    float v_node4 = Battery.Voltage;

    // Lưu lại điện áp Node để in ra serial
    Battery.NodeVoltage[0] = v_node1;
    Battery.NodeVoltage[1] = v_node2;
    Battery.NodeVoltage[2] = v_node3;
    Battery.NodeVoltage[3] = v_node4;

    Battery.CellVoltage[0] = v_node1;
    Battery.CellVoltage[1] = (v_node2 > v_node1) ? (v_node2 - v_node1) : 0.0f;
    Battery.CellVoltage[2] = (v_node3 > v_node2) ? (v_node3 - v_node2) : 0.0f;
    Battery.CellVoltage[3] = (v_node4 > v_node3) ? (v_node4 - v_node3) : 0.0f;

    // Dùng bảng tra OCV để tính chính xác phần trăm của từng cell thay vì tính xấp xỉ
    for(int i=0; i<4; i++){
        Battery.CellSOC[i] = (uint8_t)SOC_GetFromVoltage(Battery.CellVoltage[i]);
        Battery.CellSOH[i] = Battery.SOH; // Tạm thời dùng SOH chung
    }

    /* Dòng điện (mA -> A) */
    Battery.Current = INA219_ReadCurrent(Battery.INA219) / 1000.0f;
    
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
    // Bước 1: Tính điện áp trung bình của 1 cell (Pack Voltage / 4.0) để tra bảng OCV
    float avg_cell_voltage = Battery.Voltage / 4.0f;
    SOC_UpdateOCV(avg_cell_voltage);

    // Fix lỗi 0% SOC: Lúc bật máy INA219 chưa đọc kịp nên bị 0V
    // Nếu phát hiện điện áp pin có thật (>3V) mà SOC vẫn đang 0%, ta khởi tạo lại
    if (Battery.SOC == 0 && avg_cell_voltage > 3.0f) {
        SOC_Init(avg_cell_voltage);
    }

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

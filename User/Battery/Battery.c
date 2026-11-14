/******************************************************************************
 * File    : Battery.c
 * Author  : Kien
 * Brief   : Battery Management Module 
 ******************************************************************************/

#include "Battery.h"
#include "SOC.h"
#include "SOH.h"
#include <math.h>

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
    Battery.BusReadOK = 0;    Battery.CurrentReadOK = 0;

    // Đọc điện áp lần đầu để khởi tạo SOC (Chia 4 để lấy điện áp trung bình 1 cell)
    uint16_t initial_voltage_mv = 0;
    float initial_voltage = 0.0f;
    if (INA219_ReadBusVoltage(Battery.INA219, &initial_voltage_mv) == HAL_OK)
    {
        initial_voltage = initial_voltage_mv / 1000.0f;
    }
    SOC_Init(initial_voltage / 4.0f);
    
    // Khởi tạo SOH
    SOH_Init();
}

/******************************************************************************
 * @brief  Battery_Update
 * @note   Cập nhật trạng thái pin (Gọi mỗi giây)
 ******************************************************************************/
extern uint16_t adc_buffer[5];

// ==============================================================================
// HỆ SỐ CẦU CHIA ÁP (VOLTAGE DIVIDER RATIOS)
// Tỉ lệ phân áp phần cứng: Node 1 = 1/2, Node 2 = 1/6, Node 3 = 1/6, Node 4 = 1/6
// Hệ số nhân khôi phục điện áp: V_Node = V_ADC * (1 / Tỉ lệ phân áp)
// ==============================================================================
#ifndef ADC_VREF
#define ADC_VREF      3.300f   // Điện áp tham chiếu ADC (VDD 3.3V)
#endif

#ifndef NODE1_RATIO
#define NODE1_RATIO   2.018f   // Hiệu chuẩn node: TFT 3.70V, VOM 3.86V
#endif

#ifndef NODE2_RATIO
#define NODE2_RATIO   5.980f   // Hiệu chuẩn node: TFT 7.29V, VOM 7.64V
#endif

#ifndef NODE3_RATIO
#define NODE3_RATIO   6.055f   // Hiệu chuẩn node: TFT 11.21V, VOM 11.74V
#endif

#ifndef NODE4_RATIO
#define NODE4_RATIO   6.120f   // Hiệu chuẩn node: TFT 13.13V, VOM 15.84V
#endif

void Battery_Update(void)
{
    // Tự động thử kết nối lại INA219 nếu ban đầu cắm lỏng dây hoặc cắm sau
    if (!Battery.INA219Ready && Battery.INA219 != NULL)
    {
        static uint8_t ina_retry_cnt = 0;
        if (++ina_retry_cnt >= 2) // Thử lại mỗi 2 giây
        {
            ina_retry_cnt = 0;
            // Nếu ngoại vi I2C đang dính lỗi cờ (như err=0x200), reset lại ngoại vi để thử lại sạch sẽ
            if (Battery.INA219->ina219_i2c != NULL && Battery.INA219->ina219_i2c->ErrorCode != HAL_I2C_ERROR_NONE)
            {
                __HAL_RCC_I2C1_FORCE_RESET();
                HAL_Delay(2);
                __HAL_RCC_I2C1_RELEASE_RESET();
                HAL_I2C_Init(Battery.INA219->ina219_i2c);
            }
            if (INA219_Init(Battery.INA219, Battery.INA219->ina219_i2c, Battery.INA219->Address))
            {
                Battery.INA219Ready = 1;
            }
        }
    }

    /* Điện áp Bus (mV -> V) từ INA219 */
    uint16_t bus_voltage_mv = 0;
    int16_t current_ma = 0;
    Battery.BusReadOK = (INA219_ReadBusVoltage(Battery.INA219, &bus_voltage_mv) == HAL_OK);
    if (Battery.BusReadOK)
    {
        Battery.Voltage = bus_voltage_mv / 1000.0f;
    }

    /* Lấy mẫu đa điểm 8 mẫu */
    uint32_t adc_sum[5] = {0};
    for (int s = 0; s < 8; s++)
    {
        for (int ch = 1; ch < 5; ch++)
        {
            adc_sum[ch] += adc_buffer[ch];
        }
        for (volatile int d = 0; d < 500; d++);
    }

    float raw_node1 = ((float)adc_sum[1] / (8.0f * 4095.0f)) * ADC_VREF * NODE1_RATIO;
    float raw_node2 = ((float)adc_sum[2] / (8.0f * 4095.0f)) * ADC_VREF * NODE2_RATIO;
    float raw_node3 = ((float)adc_sum[3] / (8.0f * 4095.0f)) * ADC_VREF * NODE3_RATIO;
    float raw_node4_adc = ((float)adc_sum[4] / (8.0f * 4095.0f)) * ADC_VREF * NODE4_RATIO;

    /* Bộ lọc số Low-Pass Filter (EMA) thuần túy đáp ứng tức thì */
    static float f_node1 = 0.0f;
    static float f_node2 = 0.0f;
    static float f_node3 = 0.0f;
    static float f_node4 = 0.0f;
    static uint8_t filter_init = 0;

    if (!filter_init)
    {
        if (raw_node1 > 0.5f)
        {
            f_node1 = raw_node1;
            f_node2 = raw_node2;
            f_node3 = raw_node3;
            f_node4 = raw_node4_adc;
            filter_init = 1;
        }
    }
    else
    {
        // Lọc số EMA (70% cũ, 30% mới) vừa mượt vừa cập nhật ngay trong 1-2 giây
        f_node1 = f_node1 * 0.70f + raw_node1 * 0.30f;
        f_node2 = f_node2 * 0.70f + raw_node2 * 0.30f;
        f_node3 = f_node3 * 0.70f + raw_node3 * 0.30f;
        f_node4 = f_node4 * 0.70f + raw_node4_adc * 0.30f;
    }

    float v_node1 = f_node1;
    float v_node2 = f_node2;
    float v_node3 = f_node3;
    float v_node4_adc = f_node4;
    
    // Ưu tiên Pack Vol từ INA219 (nếu I2C đọc tốt). 
    // Nếu INA219 mất kết nối I2C, dự phòng lấy từ ADC PA4 (adc_buffer[4]) để hệ thống không bị tê liệt.
    if (Battery.BusReadOK)
    {
        Battery.Voltage = bus_voltage_mv / 1000.0f;
    }
    else if (v_node4_adc > 1.0f)
    {
        Battery.Voltage = v_node4_adc;
    }

    // Node 4 dùng cho tính toán cell: ưu tiên v_node4_adc để cùng hệ quy chiếu ADC với Node 1, 2, 3
    float v_node4 = (v_node4_adc > 1.0f) ? v_node4_adc : Battery.Voltage;

    // Lưu lại điện áp Node để in ra serial
    Battery.NodeVoltage[0] = v_node1;
    Battery.NodeVoltage[1] = v_node2;
    Battery.NodeVoltage[2] = v_node3;
    Battery.NodeVoltage[3] = v_node4;

    // Kiểm tra tính hợp lệ từng Node (Giới hạn an toàn pack 4S, nâng lên để không bị ngắt oan khi cell đầy 4.2V-4.3V)
    Battery.NodeFault[0] = (v_node1 > 4.8f);
    Battery.NodeFault[1] = (v_node2 > 9.6f || (v_node1 > 0.5f && v_node2 < v_node1));
    // Node 3 lỗi nếu > 14.4V, hoặc lớn hơn cả Node 4 (phi lý), hoặc nhỏ hơn Node 2
    Battery.NodeFault[2] = (v_node3 > 14.4f || (v_node4 > 1.0f && v_node3 >= v_node4) || v_node3 < v_node2);
    Battery.NodeFault[3] = (v_node4 > 19.2f || (v_node2 > 1.0f && v_node4 < v_node2));

    // --- Tính toán Cell 1 ---
    Battery.CellVoltage[0] = (!Battery.NodeFault[0] && v_node1 > 0.5f) ? v_node1 : 0.0f;

    // --- Tính toán Cell 2 ---
    if (!Battery.NodeFault[1] && v_node2 > v_node1)
    {
        float c2 = v_node2 - v_node1;
        if (c2 < 0.0f) c2 = 0.0f;
        if (c2 > 4.50f) c2 = 4.50f;
        Battery.CellVoltage[1] = c2;
    }
    else
    {
        // Cơ chế cách ly chống kéo sập Cell 2 về 0V khi ADC nhiễu hoặc sai lệch tức thời
        if (v_node2 > 2.0f)
        {
            float est_c2 = v_node2 / 2.0f;
            if (est_c2 > 4.50f) est_c2 = 4.50f;
            Battery.CellVoltage[1] = est_c2;
        }
        else
        {
            Battery.CellVoltage[1] = 0.0f;
        }
    }

    // --- Tính toán Cell 3 và Cell 4 (Có cơ chế cách ly chống kéo sập Cell khi Node 3 lỗi/bão hòa) ---
    if (!Battery.NodeFault[2] && v_node3 > v_node2 && v_node4 > v_node3)
    {
        // Node 3 hoàn toàn bình thường -> Tính toán vi sai chính xác
        float c3 = v_node3 - v_node2;
        float c4 = v_node4 - v_node3;
        Battery.CellVoltage[2] = (c3 <= 4.5f && c3 >= 0.0f) ? c3 : 0.0f;
        Battery.CellVoltage[3] = (c4 <= 4.5f && c4 >= 0.0f) ? c4 : 0.0f;
    }
    else
    {
        // Node 3 bị bão hòa (chạm 3.3V) hoặc đứt mass cầu chia áp (v_node3 >= v_node4):
        // Cách ly lỗi: Lấy phần điện áp còn lại giữa Node 4 và Node 2 chia đều cho Cell 3 & 4
        // Ngăn chặn triệt để hiện tượng C3 vọt lên 11.99V và C4 bị ép về 0.00V
        if (!Battery.NodeFault[1] && v_node4 > v_node2)
        {
            float rem_v = v_node4 - v_node2;
            float est_cell = rem_v / 2.0f;
            Battery.CellVoltage[2] = (est_cell <= 4.5f && est_cell >= 0.0f) ? est_cell : 0.0f;
            Battery.CellVoltage[3] = (est_cell <= 4.5f && est_cell >= 0.0f) ? est_cell : 0.0f;
        }
        else
        {
            Battery.CellVoltage[2] = 0.0f;
            Battery.CellVoltage[3] = 0.0f;
        }
    }

    // --- THUẬT TOÁN ĐIỀU HÒA PHÂN PHỐI TỶ LỆ (PROPORTIONAL RECONCILIATION) ---
    // TẠM THỜI TẮT BẰNG #if 0 ĐỂ HIỆU CHUẨN HỆ SỐ NODE CHUẨN XÁC THEO VOM
#if 0
    if (Battery.BusReadOK && Battery.Voltage >= 8.0f)
    {
        // Chỉ điều hòa khi cả 4 Node đều khỏe mạnh (không bị đứt dây/lỗi phần cứng)
        if (!Battery.NodeFault[0] && !Battery.NodeFault[1] && !Battery.NodeFault[2] && !Battery.NodeFault[3])
        {
            float raw_cell_sum = Battery.CellVoltage[0] + Battery.CellVoltage[1] + 
                                 Battery.CellVoltage[2] + Battery.CellVoltage[3];

            if (raw_cell_sum >= 8.0f)
            {
                float k_recon = Battery.Voltage / raw_cell_sum;
                // Chỉ hiệu chỉnh khi sai số nằm trong dung sai hợp lý (+-12%), tránh sai số thô bạo
                if (k_recon >= 0.88f && k_recon <= 1.12f)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        Battery.CellVoltage[i] *= k_recon;
                        // Kẹp giới hạn an toàn vật lý tuyệt đối (tối đa 4.35V hoặc < 0V)
                        if (Battery.CellVoltage[i] > 4.50f) Battery.CellVoltage[i] = 4.50f;
                        if (Battery.CellVoltage[i] < 0.0f)  Battery.CellVoltage[i] = 0.0f;
                    }
                }
            }
        }
    }
#endif

    // Khóa trần an toàn vật lý tuyệt đối (tối đa 4.35V để đo được vùng Over-voltage, tối thiểu 0.0V)
    for (int i = 0; i < 4; i++)
    {
        if (Battery.CellVoltage[i] > 4.50f) Battery.CellVoltage[i] = 4.50f;
        if (Battery.CellVoltage[i] < 0.0f)  Battery.CellVoltage[i] = 0.0f;
    }

    // Dùng bảng tra OCV để tính chính xác phần trăm của từng cell thay vì tính xấp xỉ
    for(int i=0; i<4; i++){
        Battery.CellSOC[i] = (uint8_t)SOC_GetFromVoltage(Battery.CellVoltage[i]);
        Battery.CellSOH[i] = Battery.SOH; // Tạm thời dùng SOH chung
    }

    /* Dòng điện (mA -> A) */
    Battery.CurrentReadOK = (INA219_ReadCurrent(Battery.INA219, &current_ma) == HAL_OK);
    if (Battery.CurrentReadOK)
    {
        Battery.Current = current_ma / 1000.0f;
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

    /* Công suất (W) tính bằng phần mềm để giữ dấu âm khi sạc */
    Battery.Power = Battery.Voltage * Battery.Current;

    /* Năng lượng (Wh), cập nhật mỗi 1 giây */
    Battery.Energy += Battery.Power / 3600.0f;
    
    // -------------------------------------------------------------
    // Bước 1: Tính điện áp trung bình của 1 cell để tra bảng OCV
    // Ưu tiên tính trực tiếp từ tổng 4 Cell thực tế (tránh sụt áp ~0.5V qua công tắc/diode của INA219)
    float sum_cell = Battery.CellVoltage[0] + Battery.CellVoltage[1] + 
                     Battery.CellVoltage[2] + Battery.CellVoltage[3];
    float avg_cell_voltage = (sum_cell > 10.0f) ? (sum_cell / 4.0f) : (Battery.Voltage / 4.0f);
    SOC_UpdateOCV(avg_cell_voltage);

    // Khởi tạo lại SOC chuẩn xác ngay chu kỳ đầu tiên có dữ liệu 4 Cell
    static uint8_t soc_calibrated_by_cells = 0;
    if (!soc_calibrated_by_cells && avg_cell_voltage > 3.0f) {
        SOC_Init(avg_cell_voltage);
        soc_calibrated_by_cells = 1;
    }

    // Bước 2: Chạy thuật toán Coulomb Counting (Tích phân dòng)
    SOC_UpdateCC(Battery.Current, 1.0f); 

    // Bước 3: Sửa lỗi trôi dạt Coulomb bằng OCV khi nghỉ ngơi
    SOC_Correction(Battery.Current);

    // Bước 4: Chạy thuật toán SOH (Đếm số chu kỳ đã xả)
    SOH_Update(Battery.Current, 1.0f);

    // Bước 5: Lấy giá trị SOC và SOH cuối cùng lưu vào struct (làm tròn số nguyên chuẩn)
    Battery.SOC = (uint8_t)(SOC_Get() + 0.5f);
    Battery.SOH = (uint8_t)(SOH_Get() + 0.5f);
    // -------------------------------------------------------------
}

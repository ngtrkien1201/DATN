#include "SOH.h"
#include "BatteryConfig.h"

SOH_t BatterySOH;

void SOH_Init(void)
{
    // Trong thực tế, các biến này phải được đọc từ bộ nhớ Flash/EEPROM của STM32
    // để ghi nhớ qua các lần tắt máy. Tạm thời khởi tạo ở mức lý tưởng.
    BatterySOH.Total_Discharged_Ah = 0.0f;
    BatterySOH.Cycles = 0.0f;
    BatterySOH.SOH_Percent = 100.0f;
}

void SOH_Update(float current, float dt)
{
    // Lấy dung lượng danh định của pin từ cấu hình (Ví dụ: 3.0Ah)
    float capacity_ah = BatteryConfig.Capacity_Ah;
    if (capacity_ah <= 0.0f) capacity_ah = 2.2f;

    // Chỉ tích lũy khi pin đang xả (dòng > 0 theo chiều INA219 của bạn)
    if (current > 0.01f) 
    {
        // Tính lượng Ah đã xả trong khoảng thời gian dt (giây)
        float discharged_ah = (current * dt) / 3600.0f;
        BatterySOH.Total_Discharged_Ah += discharged_ah;
    }

    // Tính số chu kỳ sạc xả tương đương (1 Cycle = Xả hết dung lượng danh định)
    BatterySOH.Cycles = BatterySOH.Total_Discharged_Ah / capacity_ah;

    // Thuật toán ước lượng SOH đơn giản (Linear Degradation):
    // Tuổi thọ trung bình của pin 18650 là giảm 20% dung lượng sau 500 chu kỳ
    float degradation_factor = (BatterySOH.Cycles / 500.0f) * 20.0f;
    
    BatterySOH.SOH_Percent = 100.0f - degradation_factor;

    // Giới hạn an toàn
    if(BatterySOH.SOH_Percent > 100.0f) BatterySOH.SOH_Percent = 100.0f;
    if(BatterySOH.SOH_Percent < 0.0f)   BatterySOH.SOH_Percent = 0.0f;
}

float SOH_Get(void)
{
    return BatterySOH.SOH_Percent;
}

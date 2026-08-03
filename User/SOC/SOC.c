#include "SOC.h"
#include "BatteryConfig.h" // Thêm thư viện cấu hình pin

/*----------------------------------------------------------
    Biến quản lý SOC
----------------------------------------------------------*/
SOC_t BatterySOC;

/*----------------------------------------------------------
    OCV Lookup Table
    (Lithium-ion 18650 - NMC)
----------------------------------------------------------*/
typedef struct
{
    float Voltage;
    float SOC;

} OCV_Table_t;

static const OCV_Table_t OCV_Table[] =
{
    {4.20f,100.0f},
    {4.15f,95.0f},
    {4.11f,90.0f},
    {4.08f,85.0f},
    {4.02f,80.0f},
    {3.98f,75.0f},
    {3.95f,70.0f},
    {3.91f,65.0f},
    {3.87f,60.0f},
    {3.84f,55.0f},
    {3.82f,50.0f},
    {3.80f,45.0f},
    {3.78f,40.0f},
    {3.76f,35.0f},
    {3.74f,30.0f},
    {3.72f,25.0f},
    {3.70f,20.0f},
    {3.68f,15.0f},
    {3.60f,10.0f},
    {3.50f,5.0f},
    {3.30f,0.0f}
};

/*----------------------------------------------------------
    Hàm nội bộ: Lấy SOC từ bảng tra OCV
----------------------------------------------------------*/
static float SOC_GetFromVoltage(float voltage)
{
    uint8_t i;

    if(voltage >= OCV_Table[0].Voltage)
        return 100.0f;

    if(voltage <= OCV_Table[20].Voltage)
        return 0.0f;

    for(i = 0; i < 20; i++)
    {
        if((voltage <= OCV_Table[i].Voltage) &&
           (voltage >= OCV_Table[i+1].Voltage))
        {
            // Nội suy tuyến tính đơn giản hoặc trả về mốc gần nhất
            return OCV_Table[i].SOC;
        }
    }

    return 0.0f;
}

/*----------------------------------------------------------
    Khởi tạo SOC (Gọi 1 lần lúc bật máy)
----------------------------------------------------------*/
void SOC_Init(float voltage)
{
    BatterySOC.SOC_OCV   = 0.0f;
    BatterySOC.SOC_CC    = 0.0f;
    BatterySOC.SOC_Final = 0.0f;

    // Lúc vừa bật máy, dòng điện gần như bằng 0 -> Dùng OCV làm mốc ban đầu
    BatterySOC.SOC_OCV = SOC_GetFromVoltage(voltage);

    BatterySOC.SOC_CC = BatterySOC.SOC_OCV;
    BatterySOC.SOC_Final = BatterySOC.SOC_OCV;
}

/*----------------------------------------------------------
    Cập nhật SOC theo OCV
----------------------------------------------------------*/
void SOC_UpdateOCV(float voltage)
{
    BatterySOC.SOC_OCV = SOC_GetFromVoltage(voltage);
}

/*----------------------------------------------------------
    Coulomb Counting (Đếm điện lượng)
----------------------------------------------------------*/
void SOC_UpdateCC(float current, float dt)
{
    // current (A): Dòng điện (Âm là sạc, Dương là xả tùy chiều INA219, ở code của bạn: 
    // > 0 là xả (DISCHARGING), < 0 là sạc (CHARGING). 
    // Do đó nếu > 0 (xả), SOC phải GIẢM. Chúng ta sẽ trừ đi lượng xả.
    
    // dt (s): Khoảng thời gian (ví dụ 1 giây)
    
    // Lấy dung lượng thực tế từ struct cấu hình (Cho phép thay đổi pin mà không cần sửa code)
    float capacity_ah = BatteryConfig.Capacity_Ah;
    
    // Đề phòng lỗi chia cho 0 nếu chưa gọi BatteryConfig_Init
    if (capacity_ah <= 0.0f) capacity_ah = 2.2f; 
    
    float soc_delta = (current * dt) / (capacity_ah * 3600.0f) * 100.0f;
    
    // Trừ đi vì dòng xả > 0 làm mất dung lượng, dòng sạc < 0 làm tăng dung lượng (- của - là +)
    BatterySOC.SOC_CC -= soc_delta;

    // Giới hạn trong khoảng 0-100%
    if(BatterySOC.SOC_CC > 100.0f) BatterySOC.SOC_CC = 100.0f;
    if(BatterySOC.SOC_CC < 0.0f) BatterySOC.SOC_CC = 0.0f;
}

/*----------------------------------------------------------
    OCV Correction (Kết hợp OCV và Coulomb Counting)
----------------------------------------------------------*/
static uint32_t idle_time_counter = 0; // Bộ đếm thời gian pin nghỉ

void SOC_Correction(float current)
{
    // Thuật toán kết hợp:
    // 1. Khi đang sạc/xả: Coulomb Counting cực kỳ chính xác trong thời gian ngắn -> Tin tưởng SOC_CC.
    // 2. Khi pin NGHỈ (dòng điện rất nhỏ) trong thời gian dài: Điện áp pin sẽ ổn định về mức OCV thực.
    //    Lúc này, Coulomb Counting có thể đã bị sai số tích lũy (drift) -> Tin tưởng SOC_OCV và hiệu chỉnh lại.

    if(current < 0.02f && current > -0.02f) 
    {
        idle_time_counter++;
        
        // Nếu pin nghỉ liên tục trên 60 giây (điện áp đã phục hồi và ổn định)
        if(idle_time_counter > 60)
        {
            // Thay vì gán đè ngay lập tức (làm SOC bị giật cục trên màn hình hiển thị),
            // Ta sẽ từ từ kéo SOC_CC về phía SOC_OCV (hiệu chỉnh 0.1% mỗi giây).
            if(BatterySOC.SOC_CC < BatterySOC.SOC_OCV) 
            {
                BatterySOC.SOC_CC += 0.1f;
            } 
            else if(BatterySOC.SOC_CC > BatterySOC.SOC_OCV) 
            {
                BatterySOC.SOC_CC -= 0.1f;
            }
            
            // Nếu đã chênh lệch rất nhỏ (< 0.2%), khóa chặt lại để chống nhiễu dao động
            float diff = BatterySOC.SOC_CC - BatterySOC.SOC_OCV;
            if(diff > -0.2f && diff < 0.2f) 
            {
                 BatterySOC.SOC_CC = BatterySOC.SOC_OCV;
            }
        }
    }
    else
    {
        // Có dòng sạc hoặc xả -> Đặt lại bộ đếm nghỉ
        idle_time_counter = 0;
    }

    // Giá trị cuối cùng đưa ra hệ thống là sự kết hợp hoàn hảo này
    BatterySOC.SOC_Final = BatterySOC.SOC_CC;
}

/*----------------------------------------------------------
    Lấy SOC cuối cùng
----------------------------------------------------------*/
float SOC_Get(void)
{
    return BatterySOC.SOC_Final;
}

#include "Temperature.h"
#include <math.h>

Temperature_t Temp;

void Temperature_Init(ADC_HandleTypeDef *hadc)
{
    Temp.hadc = hadc;
    Temp.Temperature = 0.0f;
    Temp.Voltage = 0.0f;
}

void Temperature_Update(void)
{
    extern uint16_t adc_buffer[5];
    
    // 1. Lấy mẫu trung bình 16 lần từ kênh ADC PA0 (adc_buffer[0])
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++)
    {
        sum += adc_buffer[0];
        for (volatile int d = 0; d < 100; d++);
    }
    float raw_voltage = ((float)sum / 16.0f) * 3.3f / 4095.0f;

    // 2. Bộ lọc số EMA mượt mà (70% cũ + 30% mới)
    static float f_volt = 0.0f;
    static uint8_t init = 0;
    if (!init)
    {
        f_volt = raw_voltage;
        init = 1;
    }
    else
    {
        f_volt = f_volt * 0.70f + raw_voltage * 0.30f;
    }

    Temp.Voltage = f_volt;

    // 3. Tính toán nhiệt độ từ điện áp thực tế
    // Dải điện áp hợp lệ khi cắm cảm biến NTC 10k: 0.15V (nóng ~120°C) đến 3.15V (lạnh ~ -15°C)
    if (Temp.Voltage >= 0.15f && Temp.Voltage <= 3.15f)
    {
        float R_NTC = (Temp.Voltage * 10000.0f) / (3.3f - Temp.Voltage);
        if (R_NTC > 50.0f)
        {
            float temp_k = 1.0f / ((1.0f / 298.15f) + (1.0f / 3950.0f) * log(R_NTC / 10000.0f));
            float t_c = temp_k - 273.15f;

            // Giới hạn dải nhiệt độ vật lý (-20°C đến 125°C)
            if (t_c >= -20.0f && t_c <= 125.0f)
            {
                Temp.Temperature = t_c;
            }
        }
    }
    else
    {
        // Điện áp < 0.15V: mất kết nối, hở chân PA0 hoặc chập mass
        Temp.Temperature = 0.0f;
    }
}

#include "Temperature.h"
#include <math.h>

Temperature_t Temp;

void Temperature_Init(ADC_HandleTypeDef *hadc)
{
    Temp.hadc = hadc;
}

void Temperature_Update(void)
{
    uint32_t adc;

    HAL_ADC_Start(Temp.hadc);

    HAL_ADC_PollForConversion(Temp.hadc,100);

    adc = HAL_ADC_GetValue(Temp.hadc);

    HAL_ADC_Stop(Temp.hadc);

    Temp.Voltage = adc * 3.3f / 4095.0f;
    
    // Tính toán nhiệt độ từ điện áp (Giả sử dùng NTC 10k, B=3950, trở kéo lên 3.3V là 10k)
    // Nếu bạn dùng cảm biến khác (như LM35), hãy sửa lại công thức này!
    if(Temp.Voltage > 0.0f && Temp.Voltage < 3.29f)
    {
        float R_NTC = (Temp.Voltage * 10000.0f) / (3.3f - Temp.Voltage);
        float temp_k = 1.0f / ( (1.0f / 298.15f) + (1.0f / 3950.0f) * log(R_NTC / 10000.0f) );
        Temp.Temperature = temp_k - 273.15f; // Chuyển từ Kelvin sang độ C
    }
    else
    {
        Temp.Temperature = 0.0f; // Tránh lỗi chia cho 0 hoặc đứt dây
    }
}

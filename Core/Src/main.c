/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "INA219.h"
#include "Battery.h"
#include "Temperature.h"
#include "BatteryConfig.h"
#include "SOH.h"
#include <stdio.h>
#include <string.h>
#include "st7789.h"
#include "fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
INA219_t ina219;
uint8_t rxData;
char rxBuffer[50];
uint8_t rxIndex = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
void TFT_UpdateUI(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
    BatteryConfig_Init(); // Khởi tạo Database pin (Phả gọi trước Battery_Init)
	INA219_Init(&ina219, &hi2c1, INA219_ADDRESS);
	Battery_Init(&ina219);

	Temperature_Init(&hadc1);
	
	ST7789_Init();
	ST7789_InvertColors(0); // Đảo ngược màu sắc (0 = tắt Inversion, sửa lỗi màu đen thành trắng)
	HAL_GPIO_WritePin(TFT_BLK_GPIO_Port, TFT_BLK_Pin, GPIO_PIN_SET); // Bật đèn nền màn hình
	ST7789_Fill_Color(BLACK);
	ST7789_WriteString(10, 10, "SYSTEM READY", Font_11x18, GREEN, BLACK);
	
	char txBuff[350];
	char jsonBuff[100];
	char statusStr[20];
	
	// Khởi động ngắt nhận UART (RX) để lắng nghe cấu hình từ bên ngoài
	HAL_UART_Receive_IT(&huart1, &rxData, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		Battery_Update();
    Temperature_Update();
    TFT_UpdateUI(); // Cập nhật màn hình TFT

		switch(Battery.Status)
{
    case BATTERY_IDLE:
        strcpy(statusStr,"IDLE");
        break;

    case BATTERY_CHARGING:
        strcpy(statusStr,"CHARGING");
        break;

    case BATTERY_DISCHARGING:
        strcpy(statusStr,"DISCHARGING");
        break;

    default:
        strcpy(statusStr,"UNKNOWN");
        break;
}

	sprintf(txBuff,
        "\r\n========== Battery ==========\r\n"
        "Voltage : %.3f V\r\n"
        "Current : %.3f A\r\n"
        "Power   : %.3f W\r\n"
        "Energy  : %.6f Wh\r\n"
        "Status  : %s\r\n"
        "SOC     : %d %%\r\n"
        "SOH     : %d %%\r\n"
        "Temp    : %.1f C\r\n"
        "=============================\r\n"
        "JSON: {\"V\":%.3f,\"I\":%.3f,\"P\":%.3f,\"E\":%.6f,\"T\":%.1f,\"SOC\":%d,\"SOH\":%d}\r\n",

        Battery.Voltage,
				Battery.Current,
				Battery.Power,
				Battery.Energy,
				statusStr,
				Battery.SOC,
                Battery.SOH,
				Temp.Temperature,
				// Dành cho JSON
				Battery.Voltage,
				Battery.Current,
				Battery.Power,
				Battery.Energy,
				Temp.Temperature,
				Battery.SOC,
                Battery.SOH);

		HAL_UART_Transmit(&huart1,
                  (uint8_t*)txBuff,
                  strlen(txBuff),
                  HAL_MAX_DELAY);

		// Gửi JSON riêng biệt để đảm bảo không bị cắt xén
		sprintf(jsonBuff,
			"JSON: {\"V\":%.3f,\"I\":%.3f,\"P\":%.3f,\"E\":%.6f,\"T\":%.1f,\"SOC\":%d,\"SOH\":%d}\r\n",
			Battery.Voltage,
			Battery.Current,
			Battery.Power,
			Battery.Energy,
			Temp.Temperature,
			Battery.SOC,
			Battery.SOH);
		HAL_UART_Transmit(&huart1,
			(uint8_t*)jsonBuff,
			strlen(jsonBuff),
			HAL_MAX_DELAY);
									
	BatteryConfig_Print(&huart1);

    // --- BẢO VỆ NGẮT UART KHỎI LỖI OVERRUN (ORE) ---
    // Nếu bạn gửi lệnh trong lúc STM32 đang bận truyền dữ liệu, 
    // phần cứng có thể báo lỗi Overrun và TỰ ĐỘNG TẮT ngắt nhận.
    // Đoạn code này sẽ ép STM32 bật lại ngắt nhận nếu phát hiện nó bị tắt.
    if(huart1.RxState == HAL_UART_STATE_READY)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart1); // Xóa cờ lỗi
        HAL_UART_Receive_IT(&huart1, &rxData, 1); // Bật lại ngắt
    }
    // -----------------------------------------------

    HAL_Delay(1000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, TFT_RST_Pin|TFT_CS_Pin|TFT_BLK_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TFT_DC_Pin TFT_RST_Pin TFT_CS_Pin TFT_BLK_Pin */
  GPIO_InitStruct.Pin = TFT_DC_Pin|TFT_RST_Pin|TFT_CS_Pin|TFT_BLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
uint8_t current_page = 0;
uint32_t last_page_switch = 0;

void TFT_DrawBatteryIcon(uint16_t x, uint16_t y, uint8_t soc)
{
    ST7789_DrawRectangle(x, y, x + 60, y + 30, WHITE);
    ST7789_DrawFilledRectangle(x + 60, y + 8, 4, 14, WHITE); // Đầu cực pin

    uint16_t color = GREEN;
    if (soc <= 20) color = RED;
    else if (soc <= 50) color = YELLOW;

    int fill_width = (soc * 56) / 100;
    if (fill_width > 0) {
        ST7789_DrawFilledRectangle(x + 2, y + 2, fill_width, 26, color);
    }
    if (fill_width < 56) {
        ST7789_DrawFilledRectangle(x + 2 + fill_width, y + 2, 56 - fill_width, 26, BLACK);
    }
}

void TFT_DrawPage_LiveSensor(void)
{
    char tftBuf[40];
    
    ST7789_WriteString(40, 5, "LIVE SENSOR DATA", Font_11x18, YELLOW, BLACK);
    ST7789_DrawLine(10, 25, 310, 25, GRAY);

    sprintf(tftBuf, "%5.2fV ", Battery.Voltage);
    ST7789_WriteString(10, 40, tftBuf, Font_16x26, CYAN, BLACK);
    
    sprintf(tftBuf, "%5.2fA ", Battery.Current);
    ST7789_WriteString(10, 80, tftBuf, Font_16x26, GREEN, BLACK);

    sprintf(tftBuf, "P: %5.2fW   E: %5.4fWh ", Battery.Power, Battery.Energy);
    ST7789_WriteString(10, 120, tftBuf, Font_11x18, WHITE, BLACK);

    TFT_DrawBatteryIcon(230, 40, Battery.SOC);
    sprintf(tftBuf, "SOC:%3d%%", Battery.SOC);
    ST7789_WriteString(225, 80, tftBuf, Font_11x18, GREEN, BLACK);

    ST7789_DrawRectangle(10, 150, 310, 230, GRAY);
    ST7789_WriteString(120, 180, "LIVE GRAPH", Font_11x18, GRAY, BLACK);
}

void TFT_DrawPage_DigitalTwin(void)
{
    char tftBuf[40];
    
    ST7789_WriteString(30, 5, "DIGITAL TWIN (EDGE AI)", Font_11x18, MAGENTA, BLACK);
    ST7789_DrawLine(10, 25, 310, 25, GRAY);

    sprintf(tftBuf, "SOH: %3d%% ", Battery.SOH);
    ST7789_WriteString(10, 40, tftBuf, Font_16x26, YELLOW, BLACK);

    sprintf(tftBuf, "Cycles: %5.1f   ", BatterySOH.Cycles);
    ST7789_WriteString(10, 80, tftBuf, Font_11x18, CYAN, BLACK);

    sprintf(tftBuf, "Total Ah: %5.2f Ah ", BatterySOH.Total_Discharged_Ah);
    ST7789_WriteString(10, 110, tftBuf, Font_11x18, CYAN, BLACK);

    char* healthStr = (Battery.SOH > 70) ? "HEALTHY " : "DEGRADED";
    uint16_t healthColor = (Battery.SOH > 70) ? GREEN : RED;
    ST7789_WriteString(10, 150, "AI Diagnosis:", Font_11x18, WHITE, BLACK);
    ST7789_WriteString(160, 150, healthStr, Font_11x18, healthColor, BLACK);
}

void TFT_DrawPage_Safety(void)
{
    char tftBuf[40];
    
    ST7789_WriteString(60, 5, "THERMAL & SAFETY", Font_11x18, RED, BLACK);
    ST7789_DrawLine(10, 25, 310, 25, GRAY);

    uint16_t tempColor = (Temp.Temperature > 45.0) ? RED : GREEN;
    sprintf(tftBuf, "Temp: %5.1fC ", Temp.Temperature);
    ST7789_WriteString(10, 40, tftBuf, Font_16x26, tempColor, BLACK);

    char* stStr = "UNKNOWN    ";
    if (Battery.Status == BATTERY_IDLE) stStr = "IDLE       ";
    else if (Battery.Status == BATTERY_CHARGING) stStr = "CHARGING   ";
    else if (Battery.Status == BATTERY_DISCHARGING) stStr = "DISCHARGING";
    
    sprintf(tftBuf, "State: %s", stStr);
    ST7789_WriteString(10, 90, tftBuf, Font_11x18, YELLOW, BLACK);

    char* alertStr = (Temp.Temperature > 45.0) ? "OVERHEAT WARNING!" : "NORMAL OPERATION ";
    uint16_t alertColor = (Temp.Temperature > 45.0) ? RED : GREEN;
    ST7789_WriteString(10, 140, "System Status:", Font_11x18, WHITE, BLACK);
    ST7789_WriteString(10, 170, alertStr, Font_11x18, alertColor, BLACK);
}

void TFT_UpdateUI(void)
{
    uint32_t current_time = HAL_GetTick();

    // Chuyển trang mỗi 5 giây
    if (current_time - last_page_switch > 5000)
    {
        last_page_switch = current_time;
        current_page++;
        if (current_page > 2) current_page = 0;
        
        // Xoá màn hình khi chuyển trang để không bị đè chữ
        ST7789_Fill_Color(BLACK);
    }

    switch (current_page)
    {
        case 0:
            TFT_DrawPage_LiveSensor();
            break;
        case 1:
            TFT_DrawPage_DigitalTwin();
            break;
        case 2:
            TFT_DrawPage_Safety();
            break;
    }
}

// Hàm ngắt nhận dữ liệu UART (Được gọi tự động mỗi khi nhận 1 byte)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        // Bỏ qua các ký tự rác (Enter, Xuống dòng)
        if(rxData != '\n' && rxData != '\r') 
        {
            if(rxIndex < sizeof(rxBuffer) - 1)
            {
                rxBuffer[rxIndex++] = rxData;
            }
        }
        else
        {
            rxIndex = 0; // Reset nếu gặp ký tự xuống dòng rỗng
        }

        rxBuffer[rxIndex] = '\0'; // Luôn kết thúc chuỗi để so sánh

        // Kiểm tra lệnh cấu hình chọn pin từ Database (Ví dụ: SET_PIN:3)
        // Chuỗi này dài chính xác 9 ký tự. Khi nhận đủ 9 ký tự, xử lý ngay lập tức!
        if(rxIndex >= 9 && strncmp(rxBuffer, "SET_PIN:", 8) == 0)
        {
            int id = 0;
            sscanf(&rxBuffer[8], "%d", &id); 
            
            if(id >= 0 && id <= 7) // Hỗ trợ từ ID 0 đến 7
            {
                BatteryConfig_Select(id);
            }
            rxIndex = 0; // Xử lý xong thì reset bộ đệm
        }
        // Vẫn hỗ trợ lệnh CAP:x.x cũ
        else if(rxIndex >= 7 && strncmp(rxBuffer, "CAP:", 4) == 0)
        {
            float new_cap = 0.0f;
            sscanf(&rxBuffer[4], "%f", &new_cap); 
            if(new_cap > 0.1f && new_cap < 50.0f) 
            {
                BatteryConfig_Update("Pin_Tu_Chinh", new_cap, 3.7f, 4.2f, 3.0f);
            }
            rxIndex = 0;
        }
        else if(rxIndex > 20) // Chống tràn bộ đệm nếu gõ sai quá dài
        {
            rxIndex = 0;
        }
        
        // Tiếp tục lắng nghe byte tiếp theo
        HAL_UART_Receive_IT(&huart1, &rxData, 1);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

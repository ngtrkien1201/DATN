/*
 * INA219.c
 *
 *  Created on: Dec 30, 2020
 *       Author: Piotr Smolen <komuch@gmail.com>
 */

#include "main.h"
#include "INA219.h"

uint16_t ina219_calibrationValue;
int16_t ina219_currentDivider_mA;
float ina219_powerMultiplier_mW;


HAL_StatusTypeDef Read16(INA219_t *ina219, uint8_t Register, uint16_t *value)
{
	uint8_t bytes[2] = {0, 0};
	HAL_StatusTypeDef status;
	if (ina219 == NULL || ina219->ina219_i2c == NULL || value == NULL) return HAL_ERROR;

	status = HAL_I2C_Mem_Read(
    ina219->ina219_i2c,
    (ina219->Address << 1),
    Register,
    I2C_MEMADD_SIZE_8BIT,
    bytes,
    2,
    1000
);
	if (status == HAL_OK) *value = ((uint16_t)bytes[0] << 8) | bytes[1];
	return status;
}


HAL_StatusTypeDef Write16(INA219_t *ina219, uint8_t Register, uint16_t Value)
{
	uint8_t addr[2];
	if (ina219 == NULL || ina219->ina219_i2c == NULL) return HAL_ERROR;
	addr[0] = (Value >> 8) & 0xff;  // upper byte
	addr[1] = (Value >> 0) & 0xff; // lower byte
	return HAL_I2C_Mem_Write(
    ina219->ina219_i2c,
    (ina219->Address << 1),
    Register,
    I2C_MEMADD_SIZE_8BIT,
    addr,
    2,
    1000
);
}

HAL_StatusTypeDef INA219_ReadBusVoltage(INA219_t *ina219, uint16_t *millivolts)
{
	uint16_t result;
	HAL_StatusTypeDef status;
	if (millivolts == NULL) return HAL_ERROR;
	status = Read16(ina219, INA219_REG_BUSVOLTAGE, &result);
	/*
	 * TI INA219 Register 02h (Bus Voltage Register):
	 * Bits [15:3]: 13-bit Bus Voltage data (BD12..BD0, 4mV LSB, 0..32764 mV range).
	 * Bit 2      : Reserved (always 0).
	 * Bit 1      : CNVR (Conversion Ready).
	 * Bit 0      : OVF  (Math Overflow Flag).
	 * Right-shifting 3 bits drops Bit 2..0 and shifts [15:3] into [12:0].
	 * Masking with 0x1FFF keeps the 13 data bits clean (up to 8191 * 4mV = 32.764V).
	 */
	if (status == HAL_OK)
	{
		*millivolts = (uint16_t)(((result >> 3) & 0x1FFFU) * 4U);
	}
	return status;
}

HAL_StatusTypeDef INA219_ReadCurrent_raw(INA219_t *ina219, int16_t *raw)
{
	uint16_t result;
	HAL_StatusTypeDef status;
	if (raw == NULL) return HAL_ERROR;
	status = Read16(ina219, INA219_REG_CURRENT, &result);
	if (status == HAL_OK) *raw = (int16_t)result;
	return status;
}

HAL_StatusTypeDef INA219_ReadCurrent(INA219_t *ina219, int16_t *milliamps)
{
	int16_t raw;
	HAL_StatusTypeDef status;
	if (milliamps == NULL || ina219_currentDivider_mA == 0) return HAL_ERROR;
	status = INA219_ReadCurrent_raw(ina219, &raw);
	if (status == HAL_OK) *milliamps = (int16_t)(raw / ina219_currentDivider_mA);
	return status;
}

float INA219_ReadShuntVoltage(INA219_t *ina219)
{
    uint16_t raw = 0;
	if (Read16(ina219, INA219_REG_SHUNTVOLTAGE, &raw) != HAL_OK) return 0.0f;
	int16_t result = (int16_t)raw;

    return result * 0.01f;
}

void INA219_Reset(INA219_t *ina219)
{
	(void)Write16(ina219, INA219_REG_CONFIG, INA219_CONFIG_RESET);
	HAL_Delay(1);
}

void INA219_setCalibration(INA219_t *ina219, uint16_t CalibrationData)
{
	(void)Write16(ina219, INA219_REG_CALIBRATION, CalibrationData);
}

uint16_t INA219_getConfig(INA219_t *ina219)
{
	uint16_t result = 0;
	(void)Read16(ina219, INA219_REG_CONFIG, &result);
	return result;
}

void INA219_setConfig(INA219_t *ina219, uint16_t Config)
{
	(void)Write16(ina219, INA219_REG_CONFIG, Config);
}

void INA219_setCalibration_32V_2A(INA219_t *ina219)
{
	uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
	             INA219_CONFIG_GAIN_8_320MV | INA219_CONFIG_BADCRES_12BIT |
	             INA219_CONFIG_SADCRES_12BIT_1S_532US |
	             INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

	ina219_calibrationValue = 4096;
	ina219_currentDivider_mA = 10; // Current LSB = 100uA per bit (1000/100 = 10)
	ina219_powerMultiplier_mW = 2.0f; // Power LSB = 1mW per bit (2/1)

	INA219_setCalibration(ina219, ina219_calibrationValue);
	INA219_setConfig(ina219, config);
}

void INA219_setCalibration_32V_1A(INA219_t *ina219)
{
	uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
	                    INA219_CONFIG_GAIN_8_320MV | INA219_CONFIG_BADCRES_12BIT |
	                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
	                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

	ina219_calibrationValue = 10240;
	ina219_currentDivider_mA = 25;    // Current LSB = 40uA per bit (1000/40 = 25)
	ina219_powerMultiplier_mW = 0.8f; // Power LSB = 800uW per bit

	INA219_setCalibration(ina219, ina219_calibrationValue);
	INA219_setConfig(ina219, config);
}

void INA219_setCalibration_16V_400mA(INA219_t *ina219)
{
	uint16_t config = INA219_CONFIG_BVOLTAGERANGE_16V |
	                    INA219_CONFIG_GAIN_1_40MV | INA219_CONFIG_BADCRES_12BIT |
	                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
	                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

	ina219_calibrationValue = 8192;
	ina219_currentDivider_mA = 20;    // Current LSB = 50uA per bit (1000/50 = 20)
	ina219_powerMultiplier_mW = 1.0f; // Power LSB = 1mW per bit

	INA219_setCalibration(ina219, ina219_calibrationValue);
	INA219_setConfig(ina219, config);
}

void INA219_setPowerMode(INA219_t *ina219, uint8_t Mode)
{
	uint16_t config = INA219_getConfig(ina219);

	switch (Mode) {
		case INA219_CONFIG_MODE_POWERDOWN:
			config = (config & ~INA219_CONFIG_MODE_MASK) | (INA219_CONFIG_MODE_POWERDOWN & INA219_CONFIG_MODE_MASK);
			INA219_setConfig(ina219, config);
			break;

		case INA219_CONFIG_MODE_SANDBVOLT_TRIGGERED:
			config = (config & ~INA219_CONFIG_MODE_MASK) | (INA219_CONFIG_MODE_SANDBVOLT_TRIGGERED & INA219_CONFIG_MODE_MASK);
			INA219_setConfig(ina219, config);
			break;

		case INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS:
			config = (config & ~INA219_CONFIG_MODE_MASK) | (INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS & INA219_CONFIG_MODE_MASK);
			INA219_setConfig(ina219, config);
			break;

		case INA219_CONFIG_MODE_ADCOFF:
			config = (config & ~INA219_CONFIG_MODE_MASK) | (INA219_CONFIG_MODE_ADCOFF & INA219_CONFIG_MODE_MASK);
			INA219_setConfig(ina219, config);
			break;
	}
}

uint8_t INA219_Init(INA219_t *ina219, I2C_HandleTypeDef *i2c, uint8_t Address)
{
	if (ina219 == NULL || i2c == NULL) return 0;
	ina219->ina219_i2c = i2c;

	ina219_currentDivider_mA = 0;
	ina219_powerMultiplier_mW = 0.0f;

	// Thử địa chỉ truyền vào trước, nếu không được thì tự quét các địa chỉ INA219 khác (0x40 .. 0x45)
	uint8_t possible_addrs[] = {Address, 0x40, 0x41, 0x44, 0x45, 0x42, 0x43};
	uint8_t detected_addr = 0;

	for (uint8_t i = 0; i < sizeof(possible_addrs); i++)
	{
		uint8_t addr = possible_addrs[i];
		if (HAL_I2C_IsDeviceReady(i2c, (uint16_t)(addr << 1), 3, 25) == HAL_OK)
		{
			detected_addr = addr;
			break;
		}
	}

	if (detected_addr != 0)
	{
		ina219->Address = detected_addr;
		INA219_Reset(ina219);
		HAL_Delay(5);
		INA219_setCalibration_32V_2A(ina219);
		return (HAL_I2C_IsDeviceReady(i2c, (uint16_t)(detected_addr << 1), 3, 25) == HAL_OK) ? 1U : 0U;
	}
	else
	{
		return 0;
	}
}

uint16_t INA219_ReadPower_raw(INA219_t *ina219)
{
    uint16_t result = 0;
	(void)Read16(ina219, INA219_REG_POWER, &result);
	return result;
}

float INA219_ReadPower(INA219_t *ina219)
{
    return (float)INA219_ReadPower_raw(ina219) * ina219_powerMultiplier_mW;
}

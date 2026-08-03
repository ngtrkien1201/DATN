#ifndef __TEMPERATURE_H
#define __TEMPERATURE_H

#include "main.h"

typedef struct
{
    ADC_HandleTypeDef *hadc;

    float Voltage;
    float Temperature;

}Temperature_t;

extern Temperature_t Temp;

void Temperature_Init(ADC_HandleTypeDef *hadc);
void Temperature_Update(void);

#endif

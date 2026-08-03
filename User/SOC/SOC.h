#ifndef __SOC_H
#define __SOC_H

#include "main.h"

/*=========================
      Kiểu dữ liệu SOC
==========================*/
typedef struct
{
    float SOC_OCV;      // SOC từ OCV

    float SOC_CC;       // SOC từ Coulomb Counting

    float SOC_Final;    // SOC cuối cùng

} SOC_t;

/*=========================
      Biến toàn cục
==========================*/
extern SOC_t BatterySOC;

/*=========================
      Hàm
==========================*/
void SOC_Init(float voltage);

void SOC_UpdateOCV(float voltage);

void SOC_UpdateCC(float current, float dt);

void SOC_Correction(float current);

float SOC_Get(void);

#endif

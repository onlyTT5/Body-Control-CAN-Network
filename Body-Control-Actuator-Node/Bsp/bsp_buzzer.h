#ifndef __BSP_BUZZER_H
#define __BSP_BUZZER_H

#include "stm32f10x.h"

void BspBuzzer_Init(void);
void BspBuzzer_Set(uint8_t on);

#endif

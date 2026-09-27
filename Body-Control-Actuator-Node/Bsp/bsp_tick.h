#ifndef __BSP_TICK_H
#define __BSP_TICK_H

#include "stm32f10x.h"

void BspTick_Init(void);
uint32_t BspTick_GetMs(void);

#endif

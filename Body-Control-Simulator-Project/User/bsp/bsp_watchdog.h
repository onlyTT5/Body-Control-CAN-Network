#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H

#include "stm32f1xx_hal.h"

void BspWatchdog_Init(void);
void BspWatchdog_Feed(void);

#endif

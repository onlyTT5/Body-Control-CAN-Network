#ifndef __BSP_WATCHDOG_H
#define __BSP_WATCHDOG_H

#include "stm32f10x.h"

void BspWatchdog_Init(void);
void BspWatchdog_Feed(void);

#endif

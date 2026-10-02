#ifndef __BSP_ULTRASONIC_H
#define __BSP_ULTRASONIC_H

#include "stm32f10x.h"

void BspUltrasonic_Init(void);
uint8_t BspUltrasonic_StartMeasurement(uint32_t current_tick_ms);
void BspUltrasonic_Task(uint32_t current_tick_ms);
uint8_t BspUltrasonic_GetResult(uint16_t *distance_mm,
                                uint8_t *valid);

#endif

#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "stm32f10x.h"

uint8_t BspCan_Init(void);

uint8_t BspCan_SendStdData(uint16_t std_id,
                           const uint8_t *data,
                           uint8_t dlc);
													 
uint8_t BspCan_ReceiveStdData(uint16_t *std_id,
                              uint8_t *data,
                              uint8_t *dlc);

uint8_t BspCan_SetRxStdDataFilter(uint16_t std_id);
#endif

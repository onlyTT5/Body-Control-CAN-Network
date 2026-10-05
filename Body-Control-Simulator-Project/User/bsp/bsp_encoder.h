#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#include "main.h"

typedef enum
{
    BSP_ENCODER_EVENT_NONE = 0,
    BSP_ENCODER_EVENT_CLOCKWISE,
    BSP_ENCODER_EVENT_COUNTERCLOCKWISE
} BspEncoderEvent;

void BspEncoder_Init(void);
BspEncoderEvent BspEncoder_Poll(void);
int32_t BspEncoder_GetPosition(void);

#endif

#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include "main.h"

typedef enum
{
    BSP_BUTTON_EVENT_NONE = 0,
    BSP_BUTTON_EVENT_SHORT_PRESS,
    BSP_BUTTON_EVENT_LONG_PRESS,
    BSP_BUTTON_EVENT_HOLD_2S
} BspButtonEvent;

void BspButton_Init(void);
uint8_t BspButton_LightWasPressed(void);
BspButtonEvent BspButton_MenuPoll(void);

#endif

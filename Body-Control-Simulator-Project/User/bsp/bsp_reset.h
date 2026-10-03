#ifndef BSP_RESET_H
#define BSP_RESET_H

#include <stdint.h>

typedef enum
{
    BSP_RESET_CAUSE_UNKNOWN = 0U,
    BSP_RESET_CAUSE_POWER,
    BSP_RESET_CAUSE_PIN,
    BSP_RESET_CAUSE_SOFTWARE,
    BSP_RESET_CAUSE_IWDG,
    BSP_RESET_CAUSE_WWDG,
    BSP_RESET_CAUSE_LOW_POWER
} BspResetCause;

void BspReset_Init(void);
BspResetCause BspReset_GetCause(void);

#endif

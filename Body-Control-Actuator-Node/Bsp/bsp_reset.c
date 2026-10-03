#include "bsp_reset.h"

static BspResetCause s_reset_cause = BSP_RESET_CAUSE_UNKNOWN;

void BspReset_Init(void)
{
    /* 多个标志可能同时置位，优先报告最具体的复位来源。 */
    if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
    {
        s_reset_cause = BSP_RESET_CAUSE_IWDG;
    }
    else if(RCC_GetFlagStatus(RCC_FLAG_WWDGRST) != RESET)
    {
        s_reset_cause = BSP_RESET_CAUSE_WWDG;
    }
    else if(RCC_GetFlagStatus(RCC_FLAG_SFTRST) != RESET)
    {
        s_reset_cause = BSP_RESET_CAUSE_SOFTWARE;
    }
    else if(RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
    {
        s_reset_cause = BSP_RESET_CAUSE_POWER;
    }
    else if(RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
    {
        s_reset_cause = BSP_RESET_CAUSE_PIN;
    }
    else if(RCC_GetFlagStatus(RCC_FLAG_LPWRRST) != RESET)
    {
        s_reset_cause = BSP_RESET_CAUSE_LOW_POWER;
    }
    else
    {
        s_reset_cause = BSP_RESET_CAUSE_UNKNOWN;
    }

    RCC_ClearFlag();
}

BspResetCause BspReset_GetCause(void)
{
    return s_reset_cause;
}

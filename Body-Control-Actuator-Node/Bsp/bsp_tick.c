#include "bsp_tick.h"

static volatile uint32_t s_tick_ms = 0U;

void BspTick_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t BspTick_GetMs(void)
{
    return s_tick_ms;
}

void SysTick_Handler(void)
{
    s_tick_ms++;
}

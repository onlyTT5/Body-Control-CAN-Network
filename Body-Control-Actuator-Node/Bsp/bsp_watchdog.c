#include "bsp_watchdog.h"

#define BSP_WATCHDOG_RELOAD_VALUE  2499U

void BspWatchdog_Init(void)
{
    /* 调试器暂停 CPU 时同时冻结 IWDG，避免单步调试触发复位。 */
    DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);

    /*
     * LSI 典型频率为 40 kHz，64 分频、重装值 2499：
     * (2499 + 1) * 64 / 40000 = 4 s。
     * IWDG_Enable() 会自动启动 LSI，不在启动路径中阻塞等待状态位。
     */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_64);
    IWDG_SetReload(BSP_WATCHDOG_RELOAD_VALUE);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

void BspWatchdog_Feed(void)
{
    IWDG_ReloadCounter();
}

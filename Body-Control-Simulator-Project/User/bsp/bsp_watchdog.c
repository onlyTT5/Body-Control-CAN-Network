#include "bsp_watchdog.h"

#define BSP_WATCHDOG_WRITE_ACCESS_KEY  0x5555U
#define BSP_WATCHDOG_RELOAD_KEY        0xAAAAU
#define BSP_WATCHDOG_START_KEY         0xCCCCU
#define BSP_WATCHDOG_PRESCALER_256     0x0006U
#define BSP_WATCHDOG_RELOAD_VALUE      624U

void BspWatchdog_Init(void)
{
    /* 调试器暂停 CPU 时冻结 IWDG，避免单步调试触发复位。 */
    __HAL_DBGMCU_FREEZE_IWDG();

    /*
     * LSI 典型频率为 40 kHz，256 分频、重装值 624：
     * (624 + 1) * 256 / 40000 = 4 s。
     */
    IWDG->KR = BSP_WATCHDOG_WRITE_ACCESS_KEY;
    IWDG->PR = BSP_WATCHDOG_PRESCALER_256;
    IWDG->RLR = BSP_WATCHDOG_RELOAD_VALUE;
    IWDG->KR = BSP_WATCHDOG_RELOAD_KEY;
    IWDG->KR = BSP_WATCHDOG_START_KEY;
}

void BspWatchdog_Feed(void)
{
    IWDG->KR = BSP_WATCHDOG_RELOAD_KEY;
}

#include "bsp_button.h"

#define BUTTON_DEBOUNCE_MS      20U
#define BUTTON_LONG_PRESS_MS    800U
#define BUTTON_CLEAR_HOLD_MS   2000U

static GPIO_PinState s_light_last_raw_state;
static GPIO_PinState s_light_stable_state;
static uint32_t s_light_last_change_tick;

static GPIO_PinState s_menu_last_raw_state;
static GPIO_PinState s_menu_stable_state;
static uint32_t s_menu_last_change_tick;
static uint32_t s_menu_press_start_tick;
static uint8_t s_menu_hold_stage;

void BspButton_Init(void)
{
    uint32_t current_tick;

    current_tick = HAL_GetTick();

    s_light_last_raw_state = HAL_GPIO_ReadPin(BTN_LIGHT_GPIO_Port,
                                              BTN_LIGHT_Pin);
    s_light_stable_state = s_light_last_raw_state;
    s_light_last_change_tick = current_tick;

    s_menu_last_raw_state = HAL_GPIO_ReadPin(BTN_MENU_GPIO_Port,
                                             BTN_MENU_Pin);
    s_menu_stable_state = s_menu_last_raw_state;
    s_menu_last_change_tick = current_tick;
    s_menu_press_start_tick = current_tick;
    s_menu_hold_stage = 0U;
}

uint8_t BspButton_LightWasPressed(void)
{
    GPIO_PinState current_raw_state;
    uint32_t current_tick;

    current_raw_state = HAL_GPIO_ReadPin(BTN_LIGHT_GPIO_Port, BTN_LIGHT_Pin);
    current_tick = HAL_GetTick();

    /* 原始电平发生变化：重新开始计算消抖时间 */
    if (current_raw_state != s_light_last_raw_state)
    {
        s_light_last_raw_state = current_raw_state;
        s_light_last_change_tick = current_tick;
    }

    /* 电平稳定超过 20ms，才认定为一次有效状态变化 */
    if ((current_tick - s_light_last_change_tick >= BUTTON_DEBOUNCE_MS) &&
        (s_light_stable_state != s_light_last_raw_state))
    {
        s_light_stable_state = s_light_last_raw_state;

        /* PB11 使用上拉输入：低电平代表按下 */
        if (s_light_stable_state == GPIO_PIN_RESET)
        {
            return 1U;
        }
    }

    return 0U;
}

BspButtonEvent BspButton_MenuPoll(void)
{
    GPIO_PinState current_raw_state;
    uint32_t current_tick;

    current_raw_state = HAL_GPIO_ReadPin(BTN_MENU_GPIO_Port, BTN_MENU_Pin);
    current_tick = HAL_GetTick();

    if(current_raw_state != s_menu_last_raw_state)
    {
        s_menu_last_raw_state = current_raw_state;
        s_menu_last_change_tick = current_tick;
    }

    if(((current_tick - s_menu_last_change_tick) >= BUTTON_DEBOUNCE_MS) &&
       (s_menu_stable_state != s_menu_last_raw_state))
    {
        s_menu_stable_state = s_menu_last_raw_state;

        if(s_menu_stable_state == GPIO_PIN_RESET)
        {
            s_menu_press_start_tick = current_tick;
            s_menu_hold_stage = 0U;
        }
        else if(s_menu_hold_stage == 0U)
        {
            return BSP_BUTTON_EVENT_SHORT_PRESS;
        }
    }

    if((s_menu_stable_state == GPIO_PIN_RESET) &&
       (s_menu_hold_stage == 0U) &&
       ((current_tick - s_menu_press_start_tick) >=
        BUTTON_LONG_PRESS_MS))
    {
        s_menu_hold_stage = 1U;
        return BSP_BUTTON_EVENT_LONG_PRESS;
    }

    if((s_menu_stable_state == GPIO_PIN_RESET) &&
       (s_menu_hold_stage == 1U) &&
       ((current_tick - s_menu_press_start_tick) >=
        BUTTON_CLEAR_HOLD_MS))
    {
        s_menu_hold_stage = 2U;
        return BSP_BUTTON_EVENT_HOLD_2S;
    }

    return BSP_BUTTON_EVENT_NONE;
}

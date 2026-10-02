#include "ui.h"
#include "oled.h"

void Ui_Init(void)
{
    OLED_Init();
}

void Ui_ShowBootSelfTest(void)
{
    OLED_Clear();

    OLED_ShowString(0, 10, "BODY CONTROL V0.4");
    OLED_ShowString(2, 20, "OLED: ON");
    OLED_ShowString(4, 20, "BTN: READY");
    OLED_ShowString(6, 20, "CAN: OFFLINE");
}

/* 只在开机自检结束后调用一次 */
void Ui_InitDashboard(void)
{
    OLED_Clear();

    /* 固定内容：不需要重复刷新 */
    OLED_ShowString(0, 10, "BODY CONTROL V0.4");
    OLED_ShowString(2, 20, "CAN: OFFLINE");
}

/* 只刷新灯光状态这一行 */
void Ui_UpdateLight(const BodyControlState *state)
{
    if (state->light_on)
    {
        OLED_ShowString(4, 20, "LIGHTA: ON ");
    }
    else
    {
        OLED_ShowString(4, 20, "LIGHTA: OFF");
    }
}

/* 只刷新心跳这一行 */
void Ui_UpdateHeartbeat(const BodyControlState *state)
{
    if (state->heartbeat_on)
    {
        OLED_ShowString(6, 20, "HB: ON  ");
    }
    else
    {
        OLED_ShowString(6, 20, "HB: OFF ");
    }
}

/* 用于首次进入主页时完整显示动态状态 */
void Ui_ShowStatus(const BodyControlState *state)
{
    Ui_UpdateCanStatus(state);
    Ui_UpdateLight(state);
    // Ui_UpdateHeartbeat(state);
	Ui_UpdateActuatorLight(state);
}

void Ui_UpdateCanStatus(const BodyControlState *state)
{
    if (state->can_online)
    {
        OLED_ShowString(2, 20, "CAN: ONLINE ");
    }
    else
    {
        OLED_ShowString(2, 20, "CAN: OFFLINE");
    }
}

void Ui_UpdateActuatorLight(const BodyControlState *state)
{
	if (state->actuator_reply_timeout != 0U)
	{
		OLED_ShowString(6, 20, "LIGHTB: ERROR");
	}
	else if (state->actuator_light_valid == 0U)
	{
		OLED_ShowString(6, 20, "LIGHTB: WAIT ");
	}
    else if (state->actuator_light_on != 0U)
    {
        OLED_ShowString(6, 20, "LIGHTB: ON   ");
    }
    else
    {
        OLED_ShowString(6, 20, "LIGHTB: OFF  ");
    }
}

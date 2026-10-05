#include "ui.h"
#include "oled.h"

static uint8_t s_can_online = 0U;
static uint8_t s_dtc_active_count = 0U;
static uint8_t s_dtc_stored_count = 0U;

static void Ui_DrawCanAndDtcStatus(void)
{
    char line[17];

    line[0] = 'C';
    line[1] = 'A';
    line[2] = 'N';
    line[3] = ':';
    line[4] = 'O';

    if(s_can_online != 0U)
    {
        line[5] = 'N';
        line[6] = ' ';
    }
    else
    {
        line[5] = 'F';
        line[6] = 'F';
    }

    line[7] = ' ';
    line[8] = 'D';
    line[9] = 'T';
    line[10] = 'C';
    line[11] = ':';
    line[12] = 'A';
    line[13] = (char)('0' + (s_dtc_active_count % 10U));
    line[14] = 'H';
    line[15] = (char)('0' + (s_dtc_stored_count % 10U));
    line[16] = '\0';

    OLED_ShowString(2, 4, line);
}

void Ui_Init(void)
{
    OLED_Init();
}

void Ui_ShowBootSelfTest(BspResetCause reset_cause)
{
    OLED_Clear();

    OLED_ShowString(0, 10, "BODY CONTROL V0.7");
    OLED_ShowString(2, 20, "OLED: ON");

    if(reset_cause == BSP_RESET_CAUSE_POWER)
    {
        OLED_ShowString(4, 20, "RST: POWER");
    }
    else if(reset_cause == BSP_RESET_CAUSE_PIN)
    {
        OLED_ShowString(4, 20, "RST: PIN  ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_SOFTWARE)
    {
        OLED_ShowString(4, 20, "RST: SOFT ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_IWDG)
    {
        OLED_ShowString(4, 20, "RST: IWDG ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_WWDG)
    {
        OLED_ShowString(4, 20, "RST: WWDG ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_LOW_POWER)
    {
        OLED_ShowString(4, 20, "RST: LOWPWR");
    }
    else
    {
        OLED_ShowString(4, 20, "RST: OTHER");
    }

    OLED_ShowString(6, 20, "CAN: OFFLINE");
}

/* 只在开机自检结束后调用一次 */
void Ui_InitDashboard(void)
{
    OLED_Clear();

    s_can_online = 0U;
    s_dtc_active_count = 0U;
    s_dtc_stored_count = 0U;

    /* 固定内容：不需要重复刷新 */
    OLED_ShowString(0, 20, "PARK: WAIT        ");
    Ui_DrawCanAndDtcStatus();
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
    s_can_online = (state->can_online != 0U) ? 1U : 0U;
    Ui_DrawCanAndDtcStatus();
}

void Ui_UpdateDtcStatus(uint8_t active_count, uint8_t stored_count)
{
    s_dtc_active_count = active_count;
    s_dtc_stored_count = stored_count;
    Ui_DrawCanAndDtcStatus();
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

void Ui_UpdateParkDistance(uint8_t received,
                           uint8_t valid,
                           uint16_t distance_mm)
{
    char line[19];
    uint16_t distance_cm;
    uint8_t digit_index;
    uint8_t i;

    if(received == 0U)
    {
        OLED_ShowString(0, 20, "PARK: WAIT        ");
        return;
    }

    if(valid == 0U)
    {
        OLED_ShowString(0, 20, "PARK: ERROR       ");
        return;
    }

    for(i = 0U; i < 18U; i++)
    {
        line[i] = ' ';
    }
    line[18] = '\0';

    line[0] = 'P';
    line[1] = 'A';
    line[2] = 'R';
    line[3] = 'K';
    line[4] = ':';
    line[10] = '.';
    line[11] = (char)('0' + (distance_mm % 10U));
    line[12] = 'C';
    line[13] = 'M';

    distance_cm = distance_mm / 10U;
    digit_index = 9U;
    do
    {
        line[digit_index] = (char)('0' + (distance_cm % 10U));
        distance_cm /= 10U;
        digit_index--;
    }
    while((distance_cm != 0U) && (digit_index >= 6U));

    OLED_ShowString(0, 20, line);
}

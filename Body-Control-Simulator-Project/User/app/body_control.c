#include "body_control.h"
#include "bsp_led.h"

#define HEARTBEAT_PERIOD_MS  1000U
#define CAN_TIMEOUT_MS  3000U
#define LIGHT_REPLY_TIMEOUT_MS  2000U

static uint32_t last_can_heartbeat_tick;

static BodyControlState state;
static uint32_t last_heartbeat_tick;

static uint8_t light_reply_pending;
static uint32_t last_light_command_tick;

static void BodyControl_StartLightReplyWait(void)
{
    state.actuator_light_valid = 0U;
    state.actuator_reply_timeout = 0U;
    light_reply_pending = 1U;
    last_light_command_tick = HAL_GetTick();
}

void BodyControl_Init(void)
{
    state.light_on = 0U;
    state.can_online = 0U;
    state.heartbeat_on = 0U;
    state.heartbeat_count = 0U;
	state.actuator_light_on = 0U;
	state.actuator_light_valid = 0U;
	state.actuator_reply_timeout = 0U;
	light_reply_pending = 0U;
    last_heartbeat_tick = HAL_GetTick();

    BspLed_Set(0U);
	last_can_heartbeat_tick = HAL_GetTick();
}

void BodyControl_ToggleLight(void)
{
	BodyControl_StartLightReplyWait();
    state.light_on = !state.light_on;
    BspLed_Set(state.light_on);
}

uint8_t BodyControl_HeartbeatTask(void)
{
    uint32_t current_tick = HAL_GetTick();

    if (current_tick - last_heartbeat_tick >= HEARTBEAT_PERIOD_MS)
    {
        last_heartbeat_tick = current_tick;

        state.heartbeat_on = !state.heartbeat_on;
        state.heartbeat_count++;

        return 1U;
    }

    return 0U;
}

const BodyControlState *BodyControl_GetState(void)
{
    return &state;
}

void BodyControl_SetLight(uint8_t light_on)
{
	BodyControl_StartLightReplyWait();
    state.light_on = light_on ? 1U : 0U;
    BspLed_Set(state.light_on);
}

void BodyControl_SetCanOnline(uint8_t online)
{
    state.can_online = online ? 1U : 0U;
}

void BodyControl_OnCanHeartbeat(uint8_t sequence, uint8_t needs_sync)
{
    (void)sequence;
	if ((state.can_online == 0U) ||
    ((needs_sync != 0U) && (light_reply_pending == 0U)))
	{
		BodyControl_StartLightReplyWait();
	}
    state.can_online = 1U;
    last_can_heartbeat_tick = HAL_GetTick();
}

uint8_t BodyControl_CanTimeoutTask(void)
{
    uint32_t current_tick = HAL_GetTick();

    if ((state.can_online == 1U) &&
        (current_tick - last_can_heartbeat_tick >= CAN_TIMEOUT_MS))
    {
        state.can_online = 0U;
		state.actuator_light_valid = 0U;
		light_reply_pending = 0U;
		state.actuator_reply_timeout = 0U;
        return 1U;
    }

    return 0U;
}

void BodyControl_OnLightStatus(uint8_t light_on)
{
    state.actuator_light_on = (light_on != 0U) ? 1U : 0U;
    state.actuator_light_valid = 1U;
	light_reply_pending = 0U;
	state.actuator_reply_timeout = 0U;
}

uint8_t BodyControl_LightReplyTimeoutTask(void)
{
    if ((state.can_online != 0U) &&
        (light_reply_pending != 0U) &&
        (state.actuator_reply_timeout == 0U) &&
        ((HAL_GetTick() - last_light_command_tick) >=
         LIGHT_REPLY_TIMEOUT_MS))
    {
        state.actuator_reply_timeout = 1U;
        return 1U;
    }

    return 0U;
}

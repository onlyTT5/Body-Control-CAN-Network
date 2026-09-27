#ifndef BODY_CONTROL_H
#define BODY_CONTROL_H

#include "main.h"

typedef struct
{
    uint8_t light_on;
    uint8_t can_online;

    uint8_t heartbeat_on;
    uint32_t heartbeat_count;
	uint8_t actuator_light_on;     /* B 回报的灯光状态 */
	uint8_t actuator_light_valid;  /* 是否收到当前命令的回报 */
	uint8_t actuator_reply_timeout;
} BodyControlState;

void BodyControl_Init(void);
void BodyControl_ToggleLight(void);

/* 每次主循环调用；仅在满 1 秒并更新心跳时返回 1U */
uint8_t BodyControl_HeartbeatTask(void);

const BodyControlState *BodyControl_GetState(void);

void BodyControl_SetLight(uint8_t light_on);
void BodyControl_SetCanOnline(uint8_t online);

void BodyControl_OnCanHeartbeat(uint8_t sequence, uint8_t needs_sync);
uint8_t BodyControl_CanTimeoutTask(void);

void BodyControl_OnLightStatus(uint8_t light_on);

uint8_t BodyControl_LightReplyTimeoutTask(void);

#endif

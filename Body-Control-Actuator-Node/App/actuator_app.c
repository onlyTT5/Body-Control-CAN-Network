#include "actuator_app.h"

#include "bsp_can.h"
#include "bsp_tick.h"
#include "bsp_buzzer.h"
#include "bsp_led.h"
#include "can_protocol.h"

static uint8_t s_light_on = 0U;

static uint8_t s_buzzer_on = 0U;
static uint32_t s_buzzer_start_tick = 0U;

static uint8_t s_heartbeat_sequence = 0U;
static uint32_t s_last_heartbeat_tick = 0U;

static CanProtocolFrame s_tx_frame;
static CanProtocolFrame s_rx_frame;

static uint8_t s_needs_sync = 1U;

uint8_t ActuatorApp_Init(void)
{
    BspLed_Init();
    BspTick_Init();
    BspBuzzer_Init();

    if(BspCan_Init() != 1U)
    {
        return 0U;
    }

    if(BspCan_SetRxStdDataFilter(CAN_ID_LIGHT_CONTROL) != 1U)
    {
        return 0U;
    }

    s_last_heartbeat_tick = BspTick_GetMs();

    return 1U;
}

void ActuatorApp_Run(void)
{
    uint32_t current_tick;
    uint8_t requested_light_on;
    uint8_t command_sequence;

    current_tick = BspTick_GetMs();

	/* 每秒发送一次节点 B 心跳 */
	if ((current_tick - s_last_heartbeat_tick) >= 1000U)
	{
		CanProtocol_BuildHeartbeat(&s_tx_frame, s_heartbeat_sequence);

		if (s_needs_sync != 0U)
		{
			s_tx_frame.data[1] |= CAN_HEARTBEAT_NEEDS_SYNC_MASK;
		}

		(void)BspCan_SendStdData(s_tx_frame.std_id,
								 s_tx_frame.data,
								 s_tx_frame.dlc);

		s_heartbeat_sequence++;
		s_last_heartbeat_tick = current_tick;
	}

    /* 接收并解析节点 A 的灯光控制帧 */
    if(BspCan_ReceiveStdData(&s_rx_frame.std_id,
                             s_rx_frame.data,
                             &s_rx_frame.dlc) == 1U)
    {
        if(CanProtocol_ParseLightControl(&s_rx_frame,
                                         &requested_light_on,
                                         &command_sequence) == 1U)
        {
            if(requested_light_on != s_light_on)
            {
                s_light_on = requested_light_on;

                BspLed_Set(s_light_on);

                BspBuzzer_Set(1U);
                s_buzzer_on = 1U;
                s_buzzer_start_tick = current_tick;
            }
			s_needs_sync = 0U;
            /* 每条有效命令都回报节点 B 当前的实际灯光状态 */
            CanProtocol_BuildLightStatus(&s_tx_frame, s_light_on,
                                         command_sequence);

            (void)BspCan_SendStdData(s_tx_frame.std_id,
                                     s_tx_frame.data,
                                     s_tx_frame.dlc);
        }
    }

    /* 蜂鸣器 100 ms 非阻塞自动关闭 */
    if((s_buzzer_on != 0U) &&
            ((current_tick - s_buzzer_start_tick) >= 100U))
    {
        BspBuzzer_Set(0U);
        s_buzzer_on = 0U;
    }
}

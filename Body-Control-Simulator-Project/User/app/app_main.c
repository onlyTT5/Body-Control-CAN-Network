#include "app_main.h"
#include "bsp_button.h"
#include "bsp_led.h"
#include "body_control.h"
#include "ui.h"
#include "can_protocol.h"
#include "bsp_can.h"

#define APP_CAN_LOOPBACK_TEST_ENABLE  0U

static void AppMain_FlashLed(uint8_t count, uint32_t delay_ms)
{
    uint8_t i;

    for(i = 0U; i < count; i++)
    {
        BspLed_Set(1U);
        HAL_Delay(delay_ms);

        BspLed_Set(0U);
        HAL_Delay(delay_ms);
    }
}

static uint8_t AppMain_CanProtocolSelfTest(void)
{
    CanProtocolFrame frame;
    uint8_t light_on;

    /* 测试 1：灯光开启帧的构造与解析 */
    CanProtocol_BuildLightControl(&frame, 1U);

    if((frame.std_id != CAN_ID_LIGHT_CONTROL) ||
            (frame.dlc != CAN_PROTOCOL_DLC) ||
            (frame.data[0] != CAN_LIGHT_ON_MASK))
    {
        return 0U;
    }

    light_on = 0U;

    if(CanProtocol_ParseLightControl(&frame, &light_on) != 1U)
    {
        return 0U;
    }

    if(light_on != 1U)
    {
        return 0U;
    }

    /* 测试 2：灯光关闭帧的构造与解析 */
    CanProtocol_BuildLightControl(&frame, 0U);
    light_on = 1U;

    if(CanProtocol_ParseLightControl(&frame, &light_on) != 1U)
    {
        return 0U;
    }

    if(light_on != 0U)
    {
        return 0U;
    }

    /* 测试 3：错误 ID 必须被拒绝 */
    frame.std_id = 0x101U;

    if(CanProtocol_ParseLightControl(&frame, &light_on) != 0U)
    {
        return 0U;
    }

    /* 测试 4：错误 DLC 必须被拒绝 */
    frame.std_id = CAN_ID_LIGHT_CONTROL;
    frame.dlc = 7U;

    if(CanProtocol_ParseLightControl(&frame, &light_on) != 0U)
    {
        return 0U;
    }

    return 1U;
}

static uint8_t AppMain_CanLoopbackSelfTest(void)
{
    CanProtocolFrame tx_frame;
    CanProtocolFrame rx_frame;

    uint16_t rx_id;
    uint8_t rx_data[CAN_PROTOCOL_DLC];
    uint8_t rx_dlc;

    uint8_t light_on;
    uint8_t i;
    uint32_t start_tick;

    /* 构造“灯光开启”的 CAN ID 0x100 报文 */
    CanProtocol_BuildLightControl(&tx_frame, 1U);

    /* 通过真实 bxCAN 外设发送；LoopBack 模式下会回到本机接收 FIFO */
    if(BspCan_SendStdData(tx_frame.std_id,
                          tx_frame.data,
                          tx_frame.dlc) != HAL_OK)
    {
        return 0U;
    }

    start_tick = HAL_GetTick();

    /* 最多等待 100ms，避免接收失败时卡死 */
    while(HAL_GetTick() - start_tick < 100U)
    {
        if(BspCan_ReceiveStdData(&rx_id, rx_data, &rx_dlc))
        {
            rx_frame.std_id = rx_id;
            rx_frame.dlc = rx_dlc;

            for(i = 0U; i < CAN_PROTOCOL_DLC; i++)
            {
                rx_frame.data[i] = 0U;
            }

            for(i = 0U; i < rx_dlc; i++)
            {
                rx_frame.data[i] = rx_data[i];
            }

            light_on = 0U;

            /* 验证收到的是 0x100 灯光帧，且解析结果为 ON */
            if((CanProtocol_ParseLightControl(&rx_frame,
                                              &light_on) == 1U) &&
                    (light_on == 1U))
            {
                return 1U;
            }
        }
    }

    return 0U;
}

static HAL_StatusTypeDef AppMain_SendLightControl(uint8_t light_on)
{
    CanProtocolFrame tx_frame;

    CanProtocol_BuildLightControl(&tx_frame, light_on);

    return BspCan_SendStdData(tx_frame.std_id,
                              tx_frame.data,
                              tx_frame.dlc);
}

static void AppMain_ProcessCanRx(void)
{
    CanProtocolFrame rx_frame;
    uint16_t rx_id;
    uint8_t rx_data[CAN_PROTOCOL_DLC];
    uint8_t rx_dlc;
    uint8_t light_on;
    uint8_t heartbeat_sequence;
    uint8_t i;
    uint8_t reported_light_on;
	uint8_t was_online;
	uint8_t needs_sync;

    while(BspCan_ReceiveStdData(&rx_id, rx_data, &rx_dlc))
    {
        rx_frame.std_id = rx_id;
        rx_frame.dlc = rx_dlc;

        for(i = 0U; i < CAN_PROTOCOL_DLC; i++)
        {
            rx_frame.data[i] = 0U;
        }

        for(i = 0U; i < rx_dlc; i++)
        {
            rx_frame.data[i] = rx_data[i];
        }

        if(CanProtocol_ParseLightControl(&rx_frame, &light_on))
        {
            BodyControl_SetLight(light_on);
			Ui_UpdateLight(BodyControl_GetState());
			Ui_UpdateActuatorLight(BodyControl_GetState());
        }
        else if (CanProtocol_ParseHeartbeat(&rx_frame, &heartbeat_sequence))
		{
			was_online = BodyControl_GetState()->can_online;
			needs_sync = ((rx_frame.data[1] &
						   CAN_HEARTBEAT_NEEDS_SYNC_MASK) != 0U) ? 1U : 0U;

			BodyControl_OnCanHeartbeat(heartbeat_sequence, needs_sync);
			Ui_UpdateCanStatus(BodyControl_GetState());

			if (needs_sync != 0U)
			{
				Ui_UpdateActuatorLight(BodyControl_GetState());
			}

			if ((was_online == 0U) || (needs_sync != 0U))
			{
				(void)AppMain_SendLightControl(
					BodyControl_GetState()->light_on
				);
			}
		}
        else if(CanProtocol_ParseLightStatus(&rx_frame,
                                             &reported_light_on))
        {
            BodyControl_OnLightStatus(reported_light_on);
            Ui_UpdateActuatorLight(BodyControl_GetState());
        }
    }
}

void AppMain_Init(void)
{
    Ui_Init();

    BspButton_Init();
    BodyControl_Init();

    Ui_ShowBootSelfTest();

    /* 板载 LED 自检 */
    AppMain_FlashLed(3U, 150U);

    /* CAN 协议软件自测：通过快闪两次；失败则常亮 1 秒 */
    if(BspCan_Init(CAN_ID_LIGHT_CONTROL,
                   CAN_ID_HEARTBEAT,
                   CAN_ID_LIGHT_STATUS) != HAL_OK)
    {
        AppMain_FlashLed(1U, 400U);
    }

#if APP_CAN_LOOPBACK_TEST_ENABLE

    else if(AppMain_CanLoopbackSelfTest())
    {
        AppMain_FlashLed(4U, 60U);
    }
    else
    {
        AppMain_FlashLed(2U, 400U);
    }

#endif

    HAL_Delay(800U);

    Ui_InitDashboard();
    Ui_ShowStatus(BodyControl_GetState());
}

void AppMain_Run(void)
{
    AppMain_ProcessCanRx();

    if(BspButton_LightWasPressed())
    {
        BodyControl_ToggleLight();
        Ui_UpdateLight(BodyControl_GetState());
		Ui_UpdateActuatorLight(BodyControl_GetState());

        (void)AppMain_SendLightControl(
            BodyControl_GetState()->light_on
        );
    }

    (void)BodyControl_HeartbeatTask();
	if (BodyControl_CanTimeoutTask())
	{
		Ui_UpdateCanStatus(BodyControl_GetState());
		Ui_UpdateActuatorLight(BodyControl_GetState());
	}
	if (BodyControl_LightReplyTimeoutTask() != 0U)
	{
		Ui_UpdateActuatorLight(BodyControl_GetState());
	}
}


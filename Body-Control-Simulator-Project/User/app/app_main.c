#include "app_main.h"
#include "bsp_button.h"
#include "bsp_led.h"
#include "body_control.h"
#include "ui.h"
#include "can_protocol.h"
#include "bsp_can.h"
#include "bsp_flash.h"
#include "event_log.h"

#define APP_CAN_PROTOCOL_SELF_TEST_ENABLE 0U
#define APP_CAN_LOOPBACK_TEST_ENABLE  0U
#define APP_FLASH_RW_SELF_TEST_ENABLE 0U
#define APP_EVENT_LOG_BOOT_TEST_ENABLE 0U
#define APP_LIGHT_RETRY_INTERVAL_MS  500U
#define APP_LIGHT_MAX_RETRIES        2U
#define APP_FLASH_TEST_ADDRESS        0x00FFF000UL

static uint8_t s_next_light_command_sequence = 0U;
static uint8_t s_pending_light_command_sequence = 0U;
static uint8_t s_light_command_pending = 0U;
static uint8_t s_pending_light_on = 0U;
static uint8_t s_light_retry_count = 0U;
static uint8_t s_last_needs_sync = 0U;
static uint8_t s_event_log_ready = 0U;
static uint32_t s_last_light_send_tick = 0U;

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

static void AppMain_LogEvent(EventLogType type,
                             uint8_t data0,
                             uint8_t data1)
{
    if(s_event_log_ready == 0U)
    {
        return;
    }

    if(EventLog_Append(type, data0, data1) != HAL_OK)
    {
        /* 本次运行停止继续写日志，避免 Flash 故障拖慢主循环。 */
        s_event_log_ready = 0U;
    }
}

#if APP_FLASH_RW_SELF_TEST_ENABLE
static uint8_t AppMain_FlashReadWriteSelfTest(void)
{
    const uint8_t test_data[16] =
    {
        0x42U, 0x43U, 0x4DU, 0x2DU,
        0x56U, 0x30U, 0x2EU, 0x34U,
        0xA5U, 0x5AU, 0x11U, 0x22U,
        0x33U, 0x44U, 0x55U, 0x66U
    };
    uint8_t read_data[16];
    uint8_t i;

    if(BspFlash_EraseSector4K(APP_FLASH_TEST_ADDRESS) != HAL_OK)
    {
        return 0U;
    }

    if(BspFlash_ProgramPage(APP_FLASH_TEST_ADDRESS,
                            test_data,
                            sizeof(test_data)) != HAL_OK)
    {
        return 0U;
    }

    if(BspFlash_Read(APP_FLASH_TEST_ADDRESS,
                     read_data,
                     sizeof(read_data)) != HAL_OK)
    {
        return 0U;
    }

    for(i = 0U; i < sizeof(test_data); i++)
    {
        if(read_data[i] != test_data[i])
        {
            return 0U;
        }
    }

    return 1U;
}
#endif

#if APP_CAN_PROTOCOL_SELF_TEST_ENABLE
static uint8_t AppMain_CanProtocolSelfTest(void)
{
    CanProtocolFrame frame;
    uint8_t light_on;
    uint8_t command_sequence;

    /* 测试 1：灯光开启帧的构造与解析 */
    CanProtocol_BuildLightControl(&frame, 1U, 0x5AU);

    if((frame.std_id != CAN_ID_LIGHT_CONTROL) ||
            (frame.dlc != CAN_PROTOCOL_DLC) ||
            (frame.data[0] != CAN_LIGHT_ON_MASK) ||
            (frame.data[1] != 0x5AU))
    {
        return 0U;
    }

    light_on = 0U;

    if(CanProtocol_ParseLightControl(&frame, &light_on,
                                     &command_sequence) != 1U)
    {
        return 0U;
    }

    if((light_on != 1U) || (command_sequence != 0x5AU))
    {
        return 0U;
    }

    /* 测试 2：灯光关闭帧的构造与解析 */
    CanProtocol_BuildLightControl(&frame, 0U, 0x5BU);
    light_on = 1U;

    if(CanProtocol_ParseLightControl(&frame, &light_on,
                                     &command_sequence) != 1U)
    {
        return 0U;
    }

    if((light_on != 0U) || (command_sequence != 0x5BU))
    {
        return 0U;
    }

    /* 测试 3：错误 ID 必须被拒绝 */
    frame.std_id = 0x101U;

    if(CanProtocol_ParseLightControl(&frame, &light_on,
                                     &command_sequence) != 0U)
    {
        return 0U;
    }

    /* 测试 4：错误 DLC 必须被拒绝 */
    frame.std_id = CAN_ID_LIGHT_CONTROL;
    frame.dlc = 7U;

    if(CanProtocol_ParseLightControl(&frame, &light_on,
                                     &command_sequence) != 0U)
    {
        return 0U;
    }

    return 1U;
}
#endif

#if APP_CAN_LOOPBACK_TEST_ENABLE
static uint8_t AppMain_CanLoopbackSelfTest(void)
{
    CanProtocolFrame tx_frame;
    CanProtocolFrame rx_frame;

    uint16_t rx_id;
    uint8_t rx_data[CAN_PROTOCOL_DLC];
    uint8_t rx_dlc;

    uint8_t light_on;
    uint8_t command_sequence;
    uint8_t i;
    uint32_t start_tick;

    /* 构造“灯光开启”的 CAN ID 0x100 报文 */
    CanProtocol_BuildLightControl(&tx_frame, 1U, 0x5AU);

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
                                              &light_on,
                                              &command_sequence) == 1U) &&
                    (light_on == 1U) &&
                    (command_sequence == 0x5AU))
            {
                return 1U;
            }
        }
    }

    return 0U;
}
#endif

static HAL_StatusTypeDef AppMain_SendLightControl(uint8_t light_on)
{
    CanProtocolFrame tx_frame;
    HAL_StatusTypeDef send_status;

    s_pending_light_on = light_on;
    s_pending_light_command_sequence = s_next_light_command_sequence;
    s_light_command_pending = 1U;
    s_light_retry_count = 0U;
    s_last_light_send_tick = HAL_GetTick();
    s_next_light_command_sequence++;

    CanProtocol_BuildLightControl(&tx_frame, light_on,
                                  s_pending_light_command_sequence);

    send_status = BspCan_SendStdData(tx_frame.std_id,
                                     tx_frame.data,
                                     tx_frame.dlc);

    if(send_status == HAL_OK)
    {
        AppMain_LogEvent(EVENT_LOG_TYPE_LIGHT_COMMAND,
                         (light_on != 0U) ? 1U : 0U,
                         s_pending_light_command_sequence);
    }

    return send_status;
}

static void AppMain_RetryLightControl(void)
{
    CanProtocolFrame tx_frame;
    uint32_t current_tick;

    if((s_light_command_pending == 0U) ||
            (s_light_retry_count >= APP_LIGHT_MAX_RETRIES) ||
            (BodyControl_GetState()->can_online == 0U))
    {
        return;
    }

    current_tick = HAL_GetTick();
    if((current_tick - s_last_light_send_tick) < APP_LIGHT_RETRY_INTERVAL_MS)
    {
        return;
    }

    CanProtocol_BuildLightControl(&tx_frame, s_pending_light_on,
                                  s_pending_light_command_sequence);
    (void)BspCan_SendStdData(tx_frame.std_id, tx_frame.data, tx_frame.dlc);
    s_light_retry_count++;
    s_last_light_send_tick = current_tick;
}

static void AppMain_SendLogResponse(uint8_t newest_offset)
{
    CanProtocolFrame tx_frame;
    EventLogRecord record;
    uint16_t stored_count;
    uint8_t status;

    stored_count = EventLog_GetStoredCount();
    status = CAN_LOG_STATUS_OK;

    if(s_event_log_ready == 0U)
    {
        status = CAN_LOG_STATUS_READ_ERROR;
    }
    else if((uint16_t)newest_offset >= stored_count)
    {
        status = CAN_LOG_STATUS_NOT_FOUND;
    }
    else if(EventLog_ReadNewest(newest_offset, &record) != HAL_OK)
    {
        status = CAN_LOG_STATUS_READ_ERROR;
    }

    if(status == CAN_LOG_STATUS_OK)
    {
        CanProtocol_BuildLogSummary(&tx_frame,
                                    status,
                                    newest_offset,
                                    (uint8_t)record.type,
                                    record.data0,
                                    record.data1,
                                    stored_count);
    }
    else
    {
        CanProtocol_BuildLogSummary(&tx_frame,
                                    status,
                                    newest_offset,
                                    0U,
                                    0U,
                                    0U,
                                    stored_count);
    }

    if(BspCan_SendStdData(tx_frame.std_id,
                          tx_frame.data,
                          tx_frame.dlc) != HAL_OK)
    {
        return;
    }

    if(status == CAN_LOG_STATUS_OK)
    {
        CanProtocol_BuildLogDetail(&tx_frame,
                                   record.sequence,
                                   record.timestamp_ms);
        (void)BspCan_SendStdData(tx_frame.std_id,
                                tx_frame.data,
                                tx_frame.dlc);
    }
}

static void AppMain_ClearLogAndRespond(void)
{
    CanProtocolFrame tx_frame;
    uint8_t status;

    status = CAN_LOG_STATUS_READ_ERROR;

    if(s_event_log_ready != 0U)
    {
        if(EventLog_Clear() == HAL_OK)
        {
            status = CAN_LOG_STATUS_OK;
        }
        else
        {
            s_event_log_ready = 0U;
        }
    }

    CanProtocol_BuildLogSummary(&tx_frame,
                                status,
                                0xFFU,
                                0U,
                                0U,
                                0U,
                                EventLog_GetStoredCount());
    (void)BspCan_SendStdData(tx_frame.std_id,
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
    uint8_t command_sequence;
    uint8_t was_online;
    uint8_t needs_sync;
    uint8_t new_sync_request;
    uint8_t log_newest_offset;

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

        if(CanProtocol_ParseLightControl(&rx_frame, &light_on,
                                         &command_sequence))
        {
            /* External CAN control: correlate B's reply, but do not retry
               a command owned by the external sender. */
            s_pending_light_command_sequence = command_sequence;
            s_pending_light_on = light_on;
            s_light_command_pending = 1U;
            s_light_retry_count = APP_LIGHT_MAX_RETRIES;
            AppMain_LogEvent(EVENT_LOG_TYPE_LIGHT_COMMAND,
                             (light_on != 0U) ? 1U : 0U,
                             command_sequence);
            BodyControl_SetLight(light_on);
            Ui_UpdateLight(BodyControl_GetState());
            Ui_UpdateActuatorLight(BodyControl_GetState());
        }
        else if(CanProtocol_ParseHeartbeat(&rx_frame, &heartbeat_sequence))
        {
            was_online = BodyControl_GetState()->can_online;
            needs_sync = ((rx_frame.data[1] &
                           CAN_HEARTBEAT_NEEDS_SYNC_MASK) != 0U) ? 1U : 0U;
            new_sync_request = ((needs_sync != 0U) &&
                                (s_last_needs_sync == 0U)) ? 1U : 0U;

            BodyControl_OnCanHeartbeat(heartbeat_sequence, new_sync_request);
            Ui_UpdateCanStatus(BodyControl_GetState());

            if(was_online == 0U)
            {
                AppMain_LogEvent(EVENT_LOG_TYPE_CAN_ONLINE,
                                 heartbeat_sequence,
                                 needs_sync);
            }

            if(new_sync_request != 0U)
            {
                Ui_UpdateActuatorLight(BodyControl_GetState());
            }

            if((was_online == 0U) ||
                    (new_sync_request != 0U))
            {
                (void)AppMain_SendLightControl(
                    BodyControl_GetState()->light_on
                );
            }
            s_last_needs_sync = needs_sync;
        }
        else if(CanProtocol_ParseLightStatus(&rx_frame,
                                             &reported_light_on,
                                             &command_sequence))
        {
            if((s_light_command_pending != 0U) &&
                    (command_sequence == s_pending_light_command_sequence))
            {
                s_light_command_pending = 0U;
                BodyControl_OnLightStatus(reported_light_on);
                AppMain_LogEvent(EVENT_LOG_TYPE_LIGHT_REPLY,
                                 (reported_light_on != 0U) ? 1U : 0U,
                                 command_sequence);
                Ui_UpdateActuatorLight(BodyControl_GetState());
            }
        }
        else if(CanProtocol_IsLogClearRequest(&rx_frame))
        {
            AppMain_ClearLogAndRespond();
        }
        else if(CanProtocol_ParseLogRequest(&rx_frame,
                                            &log_newest_offset))
        {
            AppMain_SendLogResponse(log_newest_offset);
        }
    }
}

void AppMain_Init(void)
{
    uint8_t flash_id[3];
    uint8_t flash_detected;
#if APP_EVENT_LOG_BOOT_TEST_ENABLE
    uint8_t log_flash_count;
    uint16_t stored_event_count;
#endif

    Ui_Init();

    BspButton_Init();
    BodyControl_Init();

    Ui_ShowBootSelfTest();

    /* 板载 LED 自检 */
    AppMain_FlashLed(3U, 150U);

#if APP_CAN_PROTOCOL_SELF_TEST_ENABLE
    if(AppMain_CanProtocolSelfTest() != 0U)
    {
        AppMain_FlashLed(2U, 80U);
    }
    else
    {
        AppMain_FlashLed(1U, 400U);
    }
#endif

    /* CAN 外设初始化。 */
    if(BspCan_Init(CAN_ID_LIGHT_CONTROL,
                   CAN_ID_HEARTBEAT,
                   CAN_ID_LIGHT_STATUS,
                   CAN_ID_LOG_REQUEST) != HAL_OK)
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

    /* W25Q128 JEDEC ID 自检 */
    flash_detected = 0U;
    if((BspFlash_ReadJedecId(flash_id) == HAL_OK) &&
       (BspFlash_IsW25Q128(flash_id) != 0U))
    {
        flash_detected = 1U;
        /* 读取到 EF 40 18：快速闪烁 5 次 */
        AppMain_FlashLed(5U, 80U);
    }
    else
    {
        /* SPI 通信或器件型号错误：慢闪 2 次 */
        AppMain_FlashLed(2U, 400U);
    }

#if APP_FLASH_RW_SELF_TEST_ENABLE
    if(flash_detected != 0U)
    {
        if(AppMain_FlashReadWriteSelfTest() != 0U)
        {
            /* 擦除、写入和读回比较成功：快闪 6 次。 */
            AppMain_FlashLed(6U, 60U);
        }
        else
        {
            /* 擦写或读回比较失败：慢闪 3 次。 */
            AppMain_FlashLed(3U, 400U);
        }
    }
#endif

    if(flash_detected != 0U)
    {
        if(EventLog_Init() == HAL_OK)
        {
            s_event_log_ready = 1U;
            AppMain_LogEvent(EVENT_LOG_TYPE_BOOT, 0U, 0U);
        }

        if(s_event_log_ready != 0U)
        {
#if APP_EVENT_LOG_BOOT_TEST_ENABLE
            /* 重启后依次闪 1、2、3、4、5 次，再从 1 次循环。 */
            stored_event_count = EventLog_GetStoredCount();
            log_flash_count = (uint8_t)(((stored_event_count - 1U) % 5U) + 1U);
            HAL_Delay(500U);
            AppMain_FlashLed(log_flash_count, 100U);
#endif
        }
        else
        {
            /* 日志扫描、写入或读回校验失败：慢闪 4 次。 */
            AppMain_FlashLed(4U, 400U);
        }
    }

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
    if(BodyControl_CanTimeoutTask())
    {
        s_light_command_pending = 0U;
        s_last_needs_sync = 0U;
        AppMain_LogEvent(EVENT_LOG_TYPE_CAN_OFFLINE, 0U, 0U);
        Ui_UpdateCanStatus(BodyControl_GetState());
        Ui_UpdateActuatorLight(BodyControl_GetState());
    }
    AppMain_RetryLightControl();
    if(BodyControl_LightReplyTimeoutTask() != 0U)
    {
        AppMain_LogEvent(EVENT_LOG_TYPE_LIGHT_TIMEOUT,
                         s_pending_light_on,
                         s_pending_light_command_sequence);
        Ui_UpdateActuatorLight(BodyControl_GetState());
    }
}


#include "actuator_app.h"

#include "bsp_can.h"
#include "bsp_tick.h"
#include "bsp_buzzer.h"
#include "bsp_led.h"
#include "bsp_ultrasonic.h"
#include "can_protocol.h"

#define ACTUATOR_ULTRASONIC_ENABLE           1U
#define ACTUATOR_ULTRASONIC_PERIOD_MS        100U
#define ACTUATOR_ULTRASONIC_TIMEOUT_MS       500U

#define ACTUATOR_PARK_DISTANCE_DANGER_MM     200U
#define ACTUATOR_PARK_DISTANCE_NEAR_MM       500U
#define ACTUATOR_PARK_DISTANCE_FAR_MM        1000U
#define ACTUATOR_PARK_DANGER_RELEASE_MM      230U
#define ACTUATOR_PARK_NEAR_RELEASE_MM        530U
#define ACTUATOR_PARK_FAR_RELEASE_MM         1050U

#define ACTUATOR_PARK_BUZZER_MODE_OFF        0U
#define ACTUATOR_PARK_BUZZER_MODE_SLOW       1U
#define ACTUATOR_PARK_BUZZER_MODE_FAST       2U
#define ACTUATOR_PARK_BUZZER_MODE_CONTINUOUS 3U

#define ACTUATOR_PARK_BUZZER_SLOW_TOGGLE_MS  400U
#define ACTUATOR_PARK_BUZZER_FAST_TOGGLE_MS  100U
#define ACTUATOR_LIGHT_FEEDBACK_BUZZER_MS    100U

static uint8_t s_light_on = 0U;

static uint8_t s_parking_buzzer_mode = ACTUATOR_PARK_BUZZER_MODE_OFF;
static uint8_t s_parking_buzzer_on = 0U;
static uint8_t s_light_feedback_buzzer_active = 0U;
static uint32_t s_last_parking_buzzer_toggle_tick = 0U;
static uint32_t s_light_feedback_buzzer_start_tick = 0U;

static uint8_t s_heartbeat_sequence = 0U;
static uint32_t s_last_heartbeat_tick = 0U;

static CanProtocolFrame s_tx_frame;
static CanProtocolFrame s_rx_frame;

static uint8_t s_needs_sync = 1U;

#if ACTUATOR_ULTRASONIC_ENABLE
static uint8_t s_ultrasonic_sequence = 0U;
static uint8_t s_ultrasonic_result_received = 0U;
static uint32_t s_last_ultrasonic_tick = 0U;
static uint32_t s_last_ultrasonic_result_tick = 0U;
#endif

static uint8_t ActuatorApp_GetParkingBuzzerMode(uint8_t valid,
                                                uint16_t distance_mm)
{
    uint8_t target_mode;

    if(valid == 0U)
    {
        return ACTUATOR_PARK_BUZZER_MODE_OFF;
    }

    if(distance_mm < ACTUATOR_PARK_DISTANCE_DANGER_MM)
    {
        target_mode = ACTUATOR_PARK_BUZZER_MODE_CONTINUOUS;
    }
    else if(distance_mm < ACTUATOR_PARK_DISTANCE_NEAR_MM)
    {
        target_mode = ACTUATOR_PARK_BUZZER_MODE_FAST;
    }
    else if(distance_mm < ACTUATOR_PARK_DISTANCE_FAR_MM)
    {
        target_mode = ACTUATOR_PARK_BUZZER_MODE_SLOW;
    }
    else
    {
        target_mode = ACTUATOR_PARK_BUZZER_MODE_OFF;
    }

    /* 靠近障碍物时立即升级告警。 */
    if(target_mode >= s_parking_buzzer_mode)
    {
        return target_mode;
    }

    /*
     * 远离障碍物时增加释放余量，避免测量值在边界附近抖动：
     * 常鸣需超过 230 mm、快鸣需超过 530 mm、慢鸣需超过
     * 1050 mm 才允许降低告警等级。
     */
    if((s_parking_buzzer_mode == ACTUATOR_PARK_BUZZER_MODE_CONTINUOUS) &&
       (distance_mm < ACTUATOR_PARK_DANGER_RELEASE_MM))
    {
        return s_parking_buzzer_mode;
    }

    if((s_parking_buzzer_mode == ACTUATOR_PARK_BUZZER_MODE_FAST) &&
       (distance_mm < ACTUATOR_PARK_NEAR_RELEASE_MM))
    {
        return s_parking_buzzer_mode;
    }

    if((s_parking_buzzer_mode == ACTUATOR_PARK_BUZZER_MODE_SLOW) &&
       (distance_mm < ACTUATOR_PARK_FAR_RELEASE_MM))
    {
        return s_parking_buzzer_mode;
    }

    return target_mode;
}

static void ActuatorApp_SetParkingBuzzerMode(uint8_t mode,
                                             uint32_t current_tick)
{
    if(mode == s_parking_buzzer_mode)
    {
        return;
    }

    s_parking_buzzer_mode = mode;
    s_last_parking_buzzer_toggle_tick = current_tick;

    if(mode == ACTUATOR_PARK_BUZZER_MODE_OFF)
    {
        s_parking_buzzer_on = 0U;
    }
    else
    {
        /* 进入新的告警等级时立即鸣叫，不等待第一个周期。 */
        s_parking_buzzer_on = 1U;
    }
}

static void ActuatorApp_BuzzerTask(uint32_t current_tick)
{
    uint32_t toggle_interval_ms;

    /* 灯光执行提示音优先保持 100 ms，之后恢复泊车提示。 */
    if(s_light_feedback_buzzer_active != 0U)
    {
        if((current_tick - s_light_feedback_buzzer_start_tick) <
           ACTUATOR_LIGHT_FEEDBACK_BUZZER_MS)
        {
            BspBuzzer_Set(1U);
            return;
        }

        s_light_feedback_buzzer_active = 0U;
        s_last_parking_buzzer_toggle_tick = current_tick;
    }

    if(s_parking_buzzer_mode == ACTUATOR_PARK_BUZZER_MODE_CONTINUOUS)
    {
        s_parking_buzzer_on = 1U;
    }
    else if(s_parking_buzzer_mode == ACTUATOR_PARK_BUZZER_MODE_OFF)
    {
        s_parking_buzzer_on = 0U;
    }
    else
    {
        if(s_parking_buzzer_mode == ACTUATOR_PARK_BUZZER_MODE_FAST)
        {
            toggle_interval_ms = ACTUATOR_PARK_BUZZER_FAST_TOGGLE_MS;
        }
        else
        {
            toggle_interval_ms = ACTUATOR_PARK_BUZZER_SLOW_TOGGLE_MS;
        }

        if((current_tick - s_last_parking_buzzer_toggle_tick) >=
           toggle_interval_ms)
        {
            s_last_parking_buzzer_toggle_tick = current_tick;
            s_parking_buzzer_on = (s_parking_buzzer_on == 0U) ? 1U : 0U;
        }
    }

    BspBuzzer_Set(s_parking_buzzer_on);
}

uint8_t ActuatorApp_Init(void)
{
    BspLed_Init();
    BspTick_Init();
    BspBuzzer_Init();
    BspUltrasonic_Init();

    if(BspCan_Init() != 1U)
    {
        return 0U;
    }

    if(BspCan_SetRxStdDataFilter(CAN_ID_LIGHT_CONTROL) != 1U)
    {
        return 0U;
    }

    s_last_heartbeat_tick = BspTick_GetMs();
#if ACTUATOR_ULTRASONIC_ENABLE
    s_last_ultrasonic_tick = BspTick_GetMs();
    s_last_ultrasonic_result_tick = s_last_ultrasonic_tick;
#endif

    return 1U;
}

void ActuatorApp_Run(void)
{
    uint32_t current_tick;
    uint8_t requested_light_on;
    uint8_t command_sequence;
#if ACTUATOR_ULTRASONIC_ENABLE
    uint16_t ultrasonic_distance_mm;
    uint8_t ultrasonic_valid;
    uint8_t parking_buzzer_mode;
#endif

    current_tick = BspTick_GetMs();

#if ACTUATOR_ULTRASONIC_ENABLE
    BspUltrasonic_Task(current_tick);

    if(BspUltrasonic_GetResult(&ultrasonic_distance_mm,
                               &ultrasonic_valid) != 0U)
    {
        s_ultrasonic_result_received = 1U;
        s_last_ultrasonic_result_tick = current_tick;

        parking_buzzer_mode = ActuatorApp_GetParkingBuzzerMode(
                                  ultrasonic_valid,
                                  ultrasonic_distance_mm);
        ActuatorApp_SetParkingBuzzerMode(parking_buzzer_mode,
                                         current_tick);

        CanProtocol_BuildParkDistance(&s_tx_frame,
                                      ultrasonic_valid,
                                      ultrasonic_distance_mm,
                                      s_ultrasonic_sequence);
        (void)BspCan_SendStdData(s_tx_frame.std_id,
                                 s_tx_frame.data,
                                 s_tx_frame.dlc);
        s_ultrasonic_sequence++;
    }

    if((current_tick - s_last_ultrasonic_tick) >=
       ACTUATOR_ULTRASONIC_PERIOD_MS)
    {
        if(BspUltrasonic_StartMeasurement(current_tick) != 0U)
        {
            s_last_ultrasonic_tick = current_tick;
        }
    }

    if((s_ultrasonic_result_received != 0U) &&
       ((current_tick - s_last_ultrasonic_result_tick) >=
        ACTUATOR_ULTRASONIC_TIMEOUT_MS))
    {
        s_ultrasonic_result_received = 0U;
        ActuatorApp_SetParkingBuzzerMode(ACTUATOR_PARK_BUZZER_MODE_OFF,
                                         current_tick);
    }
#endif

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

                s_light_feedback_buzzer_active = 1U;
                s_light_feedback_buzzer_start_tick = current_tick;
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

    ActuatorApp_BuzzerTask(current_tick);
}

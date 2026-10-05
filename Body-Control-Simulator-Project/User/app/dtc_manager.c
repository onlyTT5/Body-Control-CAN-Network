#include "dtc_manager.h"

#include <stddef.h>

typedef struct
{
    uint8_t failure_threshold;
    uint8_t pass_threshold;
    uint8_t auto_recover;
} DtcDebounceConfig;

static const DtcDebounceConfig s_dtc_config[DTC_CODE_COUNT] =
{
    {3U, 3U, 1U}, /* 超声波连续 3 次无效后确认，连续 3 次正常后恢复。 */
    {1U, 2U, 1U}, /* 通信超时由上层确认，连续 2 帧心跳后恢复。 */
    {2U, 3U, 1U}, /* CAN Warning 连续 2 次确认，连续 3 次正常后恢复。 */
    {2U, 3U, 1U}, /* CAN Passive 连续 2 次确认。 */
    {1U, 3U, 1U}, /* Bus-Off 严重故障立即确认。 */
    {1U, 1U, 0U}  /* 看门狗复位只作为历史事件记录。 */
};

static DtcStatus s_dtc_status[DTC_CODE_COUNT];
static DtcFreezeFrame s_freeze_frames[DTC_CODE_COUNT];

static uint8_t DtcManager_IsValidCode(DtcCode code)
{
    return ((uint8_t)code < (uint8_t)DTC_CODE_COUNT) ? 1U : 0U;
}

static void DtcManager_IncrementOccurrence(DtcStatus *status)
{
    if(status->occurrence_count < 0xFFFFU)
    {
        status->occurrence_count++;
    }
}

void DtcManager_Init(void)
{
    uint8_t i;

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        s_dtc_status[i].active = 0U;
        s_dtc_status[i].stored = 0U;
        s_dtc_status[i].freeze_frame_valid = 0U;
        s_dtc_status[i].failure_count = 0U;
        s_dtc_status[i].pass_count = 0U;
        s_dtc_status[i].occurrence_count = 0U;

        s_freeze_frames[i].health_flags = 0U;
        s_freeze_frames[i].can_last_error = 0U;
        s_freeze_frames[i].reset_cause = 0U;
        s_freeze_frames[i].distance_valid = 0U;
        s_freeze_frames[i].distance_mm = 0xFFFFU;
        s_freeze_frames[i].local_timestamp_ms = 0U;
        s_freeze_frames[i].remote_uptime_ms = 0U;
    }
}

DtcTransition DtcManager_UpdateCondition(DtcCode code,
                                         uint8_t condition_failed)
{
    const DtcDebounceConfig *config;
    DtcStatus *status;

    if(DtcManager_IsValidCode(code) == 0U)
    {
        return DTC_TRANSITION_NONE;
    }

    config = &s_dtc_config[(uint8_t)code];
    status = &s_dtc_status[(uint8_t)code];

    if(condition_failed != 0U)
    {
        status->pass_count = 0U;

        if(status->active != 0U)
        {
            return DTC_TRANSITION_NONE;
        }

        if(status->failure_count < config->failure_threshold)
        {
            status->failure_count++;
        }

        if(status->failure_count >= config->failure_threshold)
        {
            status->active = 1U;
            status->stored = 1U;
            DtcManager_IncrementOccurrence(status);
            return DTC_TRANSITION_ACTIVATED;
        }

        return DTC_TRANSITION_NONE;
    }

    status->failure_count = 0U;

    if((status->active == 0U) || (config->auto_recover == 0U))
    {
        status->pass_count = 0U;
        return DTC_TRANSITION_NONE;
    }

    if(status->pass_count < config->pass_threshold)
    {
        status->pass_count++;
    }

    if(status->pass_count >= config->pass_threshold)
    {
        status->active = 0U;
        status->pass_count = 0U;
        return DTC_TRANSITION_RECOVERED;
    }

    return DTC_TRANSITION_NONE;
}

void DtcManager_RecordHistoricalEvent(DtcCode code)
{
    DtcStatus *status;

    if(DtcManager_IsValidCode(code) == 0U)
    {
        return;
    }

    status = &s_dtc_status[(uint8_t)code];
    status->stored = 1U;
    DtcManager_IncrementOccurrence(status);
}

void DtcManager_CaptureFreezeFrame(DtcCode code,
                                   const DtcFreezeFrame *freeze_frame)
{
    if((DtcManager_IsValidCode(code) == 0U) ||
       (freeze_frame == NULL))
    {
        return;
    }

    s_freeze_frames[(uint8_t)code] = *freeze_frame;
    s_dtc_status[(uint8_t)code].freeze_frame_valid = 1U;
}

void DtcManager_RestorePersistentState(
                DtcCode code,
                uint8_t stored,
                uint16_t occurrence_count,
                const DtcFreezeFrame *freeze_frame)
{
    DtcStatus *status;

    if(DtcManager_IsValidCode(code) == 0U)
    {
        return;
    }

    status = &s_dtc_status[(uint8_t)code];

    /* 当前故障必须在本次上电后重新确认，只恢复历史信息。 */
    status->active = 0U;
    status->stored = (stored != 0U) ? 1U : 0U;
    status->freeze_frame_valid = 0U;
    status->failure_count = 0U;
    status->pass_count = 0U;
    status->occurrence_count = occurrence_count;

    if(freeze_frame != NULL)
    {
        s_freeze_frames[(uint8_t)code] = *freeze_frame;
        status->freeze_frame_valid = 1U;
    }
}

void DtcManager_ClearHistory(void)
{
    uint8_t i;

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        /* 活跃故障不能被诊断清除，必须先排除实际故障。 */
        if(s_dtc_status[i].active != 0U)
        {
            continue;
        }

        s_dtc_status[i].stored = 0U;
        s_dtc_status[i].freeze_frame_valid = 0U;
        s_dtc_status[i].failure_count = 0U;
        s_dtc_status[i].pass_count = 0U;
        s_dtc_status[i].occurrence_count = 0U;

        s_freeze_frames[i].health_flags = 0U;
        s_freeze_frames[i].can_last_error = 0U;
        s_freeze_frames[i].reset_cause = 0U;
        s_freeze_frames[i].distance_valid = 0U;
        s_freeze_frames[i].distance_mm = 0xFFFFU;
        s_freeze_frames[i].local_timestamp_ms = 0U;
        s_freeze_frames[i].remote_uptime_ms = 0U;
    }
}

uint8_t DtcManager_GetStatus(DtcCode code, DtcStatus *status)
{
    if((DtcManager_IsValidCode(code) == 0U) || (status == NULL))
    {
        return 0U;
    }

    *status = s_dtc_status[(uint8_t)code];
    return 1U;
}

uint8_t DtcManager_GetFreezeFrame(DtcCode code,
                                  DtcFreezeFrame *freeze_frame)
{
    if((DtcManager_IsValidCode(code) == 0U) ||
       (freeze_frame == NULL) ||
       (s_dtc_status[(uint8_t)code].freeze_frame_valid == 0U))
    {
        return 0U;
    }

    *freeze_frame = s_freeze_frames[(uint8_t)code];
    return 1U;
}

uint16_t DtcManager_GetActiveMask(void)
{
    uint16_t mask;
    uint8_t i;

    mask = 0U;

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        if(s_dtc_status[i].active != 0U)
        {
            mask |= (uint16_t)(1UL << i);
        }
    }

    return mask;
}

uint16_t DtcManager_GetStoredMask(void)
{
    uint16_t mask;
    uint8_t i;

    mask = 0U;

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        if(s_dtc_status[i].stored != 0U)
        {
            mask |= (uint16_t)(1UL << i);
        }
    }

    return mask;
}

uint8_t DtcManager_GetActiveCount(void)
{
    uint8_t count;
    uint8_t i;

    count = 0U;

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        if(s_dtc_status[i].active != 0U)
        {
            count++;
        }
    }

    return count;
}

uint8_t DtcManager_GetStoredCount(void)
{
    uint8_t count;
    uint8_t i;

    count = 0U;

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        if(s_dtc_status[i].stored != 0U)
        {
            count++;
        }
    }

    return count;
}

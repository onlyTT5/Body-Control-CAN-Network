#include "can_protocol.h"
#include <stddef.h>

void CanProtocol_BuildLightControl(CanProtocolFrame *frame,
                                    uint8_t light_on,
                                    uint8_t command_sequence)
{
    uint8_t i;

    if (frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_LIGHT_CONTROL;
    frame->dlc = CAN_PROTOCOL_DLC;

    for (i = 0U; i < CAN_PROTOCOL_DLC; i++)
    {
        frame->data[i] = 0U;
    }

    if (light_on)
    {
        frame->data[0] |= CAN_LIGHT_ON_MASK;
    }

    frame->data[1] = command_sequence;
}

uint8_t CanProtocol_ParseLightControl(const CanProtocolFrame *frame,
                                       uint8_t *light_on,
                                       uint8_t *command_sequence)
{
    if ((frame == NULL) || (light_on == NULL) || (command_sequence == NULL))
    {
        return 0U;
    }

    if ((frame->std_id != CAN_ID_LIGHT_CONTROL) ||
        (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *light_on = (frame->data[0] & CAN_LIGHT_ON_MASK) ? 1U : 0U;
    *command_sequence = frame->data[1];

    return 1U;
}

void CanProtocol_BuildHeartbeat(CanProtocolFrame *frame,
                                uint8_t sequence)
{
    uint8_t i;

    if (frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_HEARTBEAT;
    frame->dlc = CAN_PROTOCOL_DLC;

    for (i = 0U; i < CAN_PROTOCOL_DLC; i++)
    {
        frame->data[i] = 0U;
    }

    frame->data[0] = sequence;
}

uint8_t CanProtocol_ParseHeartbeat(const CanProtocolFrame *frame,
                                   uint8_t *sequence)
{
    if ((frame == NULL) || (sequence == NULL))
    {
        return 0U;
    }

    if ((frame->std_id != CAN_ID_HEARTBEAT) ||
        (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *sequence = frame->data[0];

    return 1U;
}

uint8_t CanProtocol_ParseLightStatus(const CanProtocolFrame *frame,
                                     uint8_t *light_on,
                                     uint8_t *command_sequence)
{
    if ((frame == NULL) || (light_on == NULL) || (command_sequence == NULL))
    {
        return 0U;
    }

    if ((frame->std_id != CAN_ID_LIGHT_STATUS) ||
        (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *light_on = ((frame->data[0] & CAN_LIGHT_ON_MASK) != 0U) ? 1U : 0U;
    *command_sequence = frame->data[1];
    return 1U;
}

uint8_t CanProtocol_ParseParkDistance(const CanProtocolFrame *frame,
                                      uint8_t *valid,
                                      uint16_t *distance_mm,
                                      uint8_t *measurement_sequence)
{
    if((frame == NULL) || (valid == NULL) ||
       (distance_mm == NULL) || (measurement_sequence == NULL))
    {
        return 0U;
    }

    if((frame->std_id != CAN_ID_PARK_DISTANCE) ||
       (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *valid = ((frame->data[0] & CAN_PARK_DISTANCE_VALID_MASK) != 0U) ?
             1U : 0U;
    *distance_mm = (uint16_t)frame->data[1] |
                   ((uint16_t)frame->data[2] << 8);
    *measurement_sequence = frame->data[3];
    return 1U;
}

uint8_t CanProtocol_ParseHealthStatus(
            const CanProtocolFrame *frame,
            CanProtocolHealthStatus *health_status)
{
    if((frame == NULL) || (health_status == NULL))
    {
        return 0U;
    }

    if((frame->std_id != CAN_ID_HEALTH_STATUS) ||
       (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    health_status->status_flags = frame->data[0];
    health_status->reset_cause = frame->data[1];
    health_status->can_last_error = frame->data[2];
    health_status->diagnostic_sequence = frame->data[3];
    health_status->uptime_ms = (uint32_t)frame->data[4] |
                               ((uint32_t)frame->data[5] << 8) |
                               ((uint32_t)frame->data[6] << 16) |
                               ((uint32_t)frame->data[7] << 24);
    return 1U;
}

uint8_t CanProtocol_ParseLogRequest(const CanProtocolFrame *frame,
                                    uint8_t *newest_offset)
{
    if((frame == NULL) || (newest_offset == NULL))
    {
        return 0U;
    }

    if((frame->std_id != CAN_ID_LOG_REQUEST) ||
       (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *newest_offset = frame->data[0];
    return 1U;
}

uint8_t CanProtocol_IsLogClearRequest(const CanProtocolFrame *frame)
{
    if(frame == NULL)
    {
        return 0U;
    }

    if((frame->std_id != CAN_ID_LOG_REQUEST) ||
       (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    /* 口令为 FF 43 4C 52 A5 5A 00 00，其中43 4C 52是ASCII的CLR。 */
    if((frame->data[0] == 0xFFU) &&
       (frame->data[1] == 0x43U) &&
       (frame->data[2] == 0x4CU) &&
       (frame->data[3] == 0x52U) &&
       (frame->data[4] == 0xA5U) &&
       (frame->data[5] == 0x5AU) &&
       (frame->data[6] == 0x00U) &&
       (frame->data[7] == 0x00U))
    {
        return 1U;
    }

    return 0U;
}

void CanProtocol_BuildLogSummary(CanProtocolFrame *frame,
                                 uint8_t status,
                                 uint8_t newest_offset,
                                 uint8_t event_type,
                                 uint8_t event_data0,
                                 uint8_t event_data1,
                                 uint16_t stored_count)
{
    uint8_t i;

    if(frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_LOG_SUMMARY;
    frame->dlc = CAN_PROTOCOL_DLC;

    for(i = 0U; i < CAN_PROTOCOL_DLC; i++)
    {
        frame->data[i] = 0U;
    }

    frame->data[0] = status;
    frame->data[1] = newest_offset;
    frame->data[2] = event_type;
    frame->data[3] = event_data0;
    frame->data[4] = event_data1;
    frame->data[5] = (uint8_t)stored_count;
    frame->data[6] = (uint8_t)(stored_count >> 8);
}

void CanProtocol_BuildLogDetail(CanProtocolFrame *frame,
                                uint32_t sequence,
                                uint32_t timestamp_ms)
{
    if(frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_LOG_DETAIL;
    frame->dlc = CAN_PROTOCOL_DLC;
    frame->data[0] = (uint8_t)sequence;
    frame->data[1] = (uint8_t)(sequence >> 8);
    frame->data[2] = (uint8_t)(sequence >> 16);
    frame->data[3] = (uint8_t)(sequence >> 24);
    frame->data[4] = (uint8_t)timestamp_ms;
    frame->data[5] = (uint8_t)(timestamp_ms >> 8);
    frame->data[6] = (uint8_t)(timestamp_ms >> 16);
    frame->data[7] = (uint8_t)(timestamp_ms >> 24);
}

uint8_t CanProtocol_ParseDtcRequest(const CanProtocolFrame *frame,
                                    uint8_t *selector)
{
    if((frame == NULL) || (selector == NULL))
    {
        return 0U;
    }

    if((frame->std_id != CAN_ID_DTC_REQUEST) ||
       (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *selector = frame->data[0];
    return 1U;
}

uint8_t CanProtocol_IsDtcClearRequest(const CanProtocolFrame *frame)
{
    if((frame == NULL) ||
       (frame->std_id != CAN_ID_DTC_REQUEST) ||
       (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    return ((frame->data[0] == CAN_DTC_COMMAND_CLEAR) &&
            (frame->data[1] == 0x43U) &&
            (frame->data[2] == 0x4CU) &&
            (frame->data[3] == 0x52U) &&
            (frame->data[4] == 0xA5U) &&
            (frame->data[5] == 0x5AU) &&
            (frame->data[6] == 0U) &&
            (frame->data[7] == 0U)) ? 1U : 0U;
}

void CanProtocol_BuildDtcSummary(CanProtocolFrame *frame,
                                 uint16_t active_mask,
                                 uint16_t stored_mask,
                                 uint8_t active_count,
                                 uint8_t stored_count)
{
    if(frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_DTC_RESPONSE;
    frame->dlc = CAN_PROTOCOL_DLC;
    frame->data[0] = CAN_DTC_STATUS_OK;
    frame->data[1] = CAN_DTC_QUERY_SUMMARY;
    frame->data[2] = (uint8_t)active_mask;
    frame->data[3] = (uint8_t)(active_mask >> 8);
    frame->data[4] = (uint8_t)stored_mask;
    frame->data[5] = (uint8_t)(stored_mask >> 8);
    frame->data[6] = active_count;
    frame->data[7] = stored_count;
}

void CanProtocol_BuildDtcClearResponse(CanProtocolFrame *frame,
                                       uint8_t status,
                                       uint16_t active_mask,
                                       uint16_t stored_mask,
                                       uint8_t active_count,
                                       uint8_t stored_count)
{
    if(frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_DTC_RESPONSE;
    frame->dlc = CAN_PROTOCOL_DLC;
    frame->data[0] = status;
    frame->data[1] = CAN_DTC_COMMAND_CLEAR;
    frame->data[2] = (uint8_t)active_mask;
    frame->data[3] = (uint8_t)(active_mask >> 8);
    frame->data[4] = (uint8_t)stored_mask;
    frame->data[5] = (uint8_t)(stored_mask >> 8);
    frame->data[6] = active_count;
    frame->data[7] = stored_count;
}

void CanProtocol_BuildDtcDetail(CanProtocolFrame *frame,
                                uint8_t status,
                                uint8_t dtc_code,
                                uint8_t active,
                                uint8_t stored,
                                uint8_t failure_count,
                                uint8_t pass_count,
                                uint16_t occurrence_count)
{
    if(frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_DTC_RESPONSE;
    frame->dlc = CAN_PROTOCOL_DLC;
    frame->data[0] = status;
    frame->data[1] = dtc_code;
    frame->data[2] = active;
    frame->data[3] = stored;
    frame->data[4] = failure_count;
    frame->data[5] = pass_count;
    frame->data[6] = (uint8_t)occurrence_count;
    frame->data[7] = (uint8_t)(occurrence_count >> 8);
}

void CanProtocol_BuildDtcFreezeData(CanProtocolFrame *frame,
                                    uint8_t status,
                                    uint8_t dtc_code,
                                    uint8_t health_flags,
                                    uint8_t can_last_error,
                                    uint8_t reset_cause,
                                    uint8_t distance_valid,
                                    uint16_t distance_mm)
{
    if(frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_DTC_FREEZE_DATA;
    frame->dlc = CAN_PROTOCOL_DLC;
    frame->data[0] = status;
    frame->data[1] = dtc_code;
    frame->data[2] = health_flags;
    frame->data[3] = can_last_error;
    frame->data[4] = reset_cause;
    frame->data[5] = distance_valid;
    frame->data[6] = (uint8_t)distance_mm;
    frame->data[7] = (uint8_t)(distance_mm >> 8);
}

void CanProtocol_BuildDtcFreezeTime(CanProtocolFrame *frame,
                                    uint8_t dtc_code,
                                    uint16_t local_timestamp_s,
                                    uint16_t remote_uptime_s)
{
    if(frame == NULL)
    {
        return;
    }

    frame->std_id = CAN_ID_DTC_FREEZE_TIME;
    frame->dlc = CAN_PROTOCOL_DLC;
    frame->data[0] = CAN_DTC_STATUS_OK;
    frame->data[1] = dtc_code;
    frame->data[2] = (uint8_t)local_timestamp_s;
    frame->data[3] = (uint8_t)(local_timestamp_s >> 8);
    frame->data[4] = (uint8_t)remote_uptime_s;
    frame->data[5] = (uint8_t)(remote_uptime_s >> 8);
    frame->data[6] = 0U;
    frame->data[7] = 0U;
}

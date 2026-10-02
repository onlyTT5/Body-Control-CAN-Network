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

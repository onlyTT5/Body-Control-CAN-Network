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

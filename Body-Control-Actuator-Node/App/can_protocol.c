#include "can_protocol.h"

void CanProtocol_BuildHeartbeat(CanProtocolFrame *frame,
                                uint8_t sequence)
{
    uint8_t i;

    if (frame == 0)
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

uint8_t CanProtocol_ParseLightControl(const CanProtocolFrame *frame,
                                      uint8_t *light_on)
{
    if ((frame == 0) || (light_on == 0))
    {
        return 0U;
    }

    if ((frame->std_id != CAN_ID_LIGHT_CONTROL) ||
        (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *light_on = ((frame->data[0] & CAN_LIGHT_ON_MASK) != 0U) ? 1U : 0U;

    return 1U;
}

void CanProtocol_BuildLightStatus(CanProtocolFrame *frame,
                                  uint8_t light_on)
{
    uint8_t i;

    if (frame == 0)
    {
        return;
    }

    frame->std_id = CAN_ID_LIGHT_STATUS;
    frame->dlc = CAN_PROTOCOL_DLC;

    for (i = 0U; i < CAN_PROTOCOL_DLC; i++)
    {
        frame->data[i] = 0U;
    }

    frame->data[0] = (light_on != 0U) ? CAN_LIGHT_ON_MASK : 0U;
}
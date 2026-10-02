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
                                      uint8_t *light_on,
                                      uint8_t *command_sequence)
{
    if ((frame == 0) || (light_on == 0) || (command_sequence == 0))
    {
        return 0U;
    }

    if ((frame->std_id != CAN_ID_LIGHT_CONTROL) ||
        (frame->dlc != CAN_PROTOCOL_DLC))
    {
        return 0U;
    }

    *light_on = ((frame->data[0] & CAN_LIGHT_ON_MASK) != 0U) ? 1U : 0U;
    *command_sequence = frame->data[1];

    return 1U;
}

void CanProtocol_BuildLightStatus(CanProtocolFrame *frame,
                                  uint8_t light_on,
                                  uint8_t command_sequence)
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
    frame->data[1] = command_sequence;
}

void CanProtocol_BuildParkDistance(CanProtocolFrame *frame,
                                   uint8_t valid,
                                   uint16_t distance_mm,
                                   uint8_t measurement_sequence)
{
    uint8_t i;

    if(frame == 0)
    {
        return;
    }

    frame->std_id = CAN_ID_PARK_DISTANCE;
    frame->dlc = CAN_PROTOCOL_DLC;

    for(i = 0U; i < CAN_PROTOCOL_DLC; i++)
    {
        frame->data[i] = 0U;
    }

    frame->data[0] = (valid != 0U) ?
                     CAN_PARK_DISTANCE_VALID_MASK : 0U;
    frame->data[1] = (uint8_t)distance_mm;
    frame->data[2] = (uint8_t)(distance_mm >> 8);
    frame->data[3] = measurement_sequence;
}

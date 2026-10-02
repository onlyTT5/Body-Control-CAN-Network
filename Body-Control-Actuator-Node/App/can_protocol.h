#ifndef __CAN_PROTOCOL_H
#define __CAN_PROTOCOL_H

#include "stm32f10x.h"

#define CAN_PROTOCOL_DLC          8U

#define CAN_ID_LIGHT_CONTROL      0x100U
#define CAN_ID_HEARTBEAT          0x101U
#define CAN_ID_LIGHT_STATUS  0x102U
#define CAN_ID_PARK_DISTANCE      0x111U

#define CAN_LIGHT_ON_MASK         0x01U

#define CAN_HEARTBEAT_NEEDS_SYNC_MASK  0x01U
#define CAN_PARK_DISTANCE_VALID_MASK   0x01U

typedef struct
{
    uint16_t std_id;
    uint8_t dlc;
    uint8_t data[CAN_PROTOCOL_DLC];
} CanProtocolFrame;

void CanProtocol_BuildHeartbeat(CanProtocolFrame *frame,
                                uint8_t sequence);

uint8_t CanProtocol_ParseLightControl(const CanProtocolFrame *frame,
                                      uint8_t *light_on,
                                      uint8_t *command_sequence);

void CanProtocol_BuildLightStatus(CanProtocolFrame *frame,
                                  uint8_t light_on,
                                  uint8_t command_sequence);

void CanProtocol_BuildParkDistance(CanProtocolFrame *frame,
                                   uint8_t valid,
                                   uint16_t distance_mm,
                                   uint8_t measurement_sequence);

#endif

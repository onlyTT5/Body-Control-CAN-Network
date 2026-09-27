#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

/* CAN 报文定义 */
#define CAN_PROTOCOL_DLC        8U
#define CAN_ID_LIGHT_CONTROL    0x100U

/* Byte0 中的灯光控制位 */
#define CAN_LIGHT_ON_MASK       0x01U

/* 0x101 心跳帧 */
#define CAN_ID_HEARTBEAT        0x101U

#define CAN_ID_LIGHT_STATUS  0x102U

#define CAN_HEARTBEAT_NEEDS_SYNC_MASK  0x01U

typedef struct
{
    uint16_t std_id;                    /* 11 位标准 CAN ID */
    uint8_t dlc;                        /* 数据长度 */
    uint8_t data[CAN_PROTOCOL_DLC];     /* 最多 8 字节 */
} CanProtocolFrame;

/* 打包：灯光状态 -> CAN 灯光控制帧 */
void CanProtocol_BuildLightControl(CanProtocolFrame *frame,
                                    uint8_t light_on,
                                    uint8_t command_sequence);

/* 解析：CAN 灯光控制帧 -> 灯光状态
   返回 1U 表示该报文有效；返回 0U 表示 ID 或长度不匹配 */
uint8_t CanProtocol_ParseLightControl(const CanProtocolFrame *frame,
                                       uint8_t *light_on,
                                       uint8_t *command_sequence);

void CanProtocol_BuildHeartbeat(CanProtocolFrame *frame,
                                uint8_t sequence);

uint8_t CanProtocol_ParseHeartbeat(const CanProtocolFrame *frame,
                                   uint8_t *sequence);

uint8_t CanProtocol_ParseLightStatus(const CanProtocolFrame *frame,
                                     uint8_t *light_on,
                                     uint8_t *command_sequence);

#endif

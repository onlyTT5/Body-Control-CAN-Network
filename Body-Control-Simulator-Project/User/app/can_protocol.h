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
#define CAN_ID_LOG_REQUEST   0x103U
#define CAN_ID_LOG_SUMMARY   0x104U
#define CAN_ID_LOG_DETAIL    0x105U
#define CAN_ID_PARK_DISTANCE 0x111U
#define CAN_ID_HEALTH_STATUS 0x114U

#define CAN_LOG_STATUS_OK          0U
#define CAN_LOG_STATUS_NOT_FOUND   1U
#define CAN_LOG_STATUS_READ_ERROR  2U

#define CAN_HEARTBEAT_NEEDS_SYNC_MASK  0x01U
#define CAN_PARK_DISTANCE_VALID_MASK   0x01U

#define CAN_HEALTH_ULTRASONIC_INVALID_MASK  0x01U
#define CAN_HEALTH_CAN_WARNING_MASK        0x02U
#define CAN_HEALTH_CAN_PASSIVE_MASK        0x04U
#define CAN_HEALTH_CAN_BUS_OFF_MASK        0x08U
#define CAN_HEALTH_IWDG_RESET_MASK         0x10U
#define CAN_HEALTH_ACTIVE_FAULT_MASK       0x0FU

#define CAN_HEALTH_RESET_CAUSE_IWDG        4U
#define CAN_HEALTH_RESET_CAUSE_WWDG        5U

typedef struct
{
    uint16_t std_id;                    /* 11 位标准 CAN ID */
    uint8_t dlc;                        /* 数据长度 */
    uint8_t data[CAN_PROTOCOL_DLC];     /* 最多 8 字节 */
} CanProtocolFrame;

typedef struct
{
    uint8_t status_flags;
    uint8_t reset_cause;
    uint8_t can_last_error;
    uint8_t diagnostic_sequence;
    uint32_t uptime_ms;
} CanProtocolHealthStatus;

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

uint8_t CanProtocol_ParseParkDistance(const CanProtocolFrame *frame,
                                      uint8_t *valid,
                                      uint16_t *distance_mm,
                                      uint8_t *measurement_sequence);

uint8_t CanProtocol_ParseHealthStatus(
            const CanProtocolFrame *frame,
            CanProtocolHealthStatus *health_status);

uint8_t CanProtocol_ParseLogRequest(const CanProtocolFrame *frame,
                                    uint8_t *newest_offset);

uint8_t CanProtocol_IsLogClearRequest(const CanProtocolFrame *frame);

void CanProtocol_BuildLogSummary(CanProtocolFrame *frame,
                                 uint8_t status,
                                 uint8_t newest_offset,
                                 uint8_t event_type,
                                 uint8_t event_data0,
                                 uint8_t event_data1,
                                 uint16_t stored_count);

void CanProtocol_BuildLogDetail(CanProtocolFrame *frame,
                                uint32_t sequence,
                                uint32_t timestamp_ms);

#endif

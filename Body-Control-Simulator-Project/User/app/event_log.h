#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include "stm32f1xx_hal.h"

typedef enum
{
    EVENT_LOG_TYPE_BOOT = 1U,
    EVENT_LOG_TYPE_CAN_ONLINE = 2U,
    EVENT_LOG_TYPE_CAN_OFFLINE = 3U,
    EVENT_LOG_TYPE_LIGHT_COMMAND = 4U,
    EVENT_LOG_TYPE_LIGHT_REPLY = 5U,
    EVENT_LOG_TYPE_LIGHT_TIMEOUT = 6U
} EventLogType;

typedef struct
{
    EventLogType type;
    uint32_t sequence;
    uint32_t timestamp_ms;
    uint8_t data0;
    uint8_t data1;
} EventLogRecord;

HAL_StatusTypeDef EventLog_Init(void);
HAL_StatusTypeDef EventLog_Append(EventLogType type,
                                  uint8_t data0,
                                  uint8_t data1);
uint16_t EventLog_GetStoredCount(void);
HAL_StatusTypeDef EventLog_ReadNewest(uint8_t newest_offset,
                                      EventLogRecord *record);
HAL_StatusTypeDef EventLog_Clear(void);

#endif

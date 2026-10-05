#ifndef DTC_MANAGER_H
#define DTC_MANAGER_H

#include <stdint.h>

typedef enum
{
    DTC_CODE_ULTRASONIC_INVALID = 0U,
    DTC_CODE_NODE_B_OFFLINE,
    DTC_CODE_CAN_WARNING,
    DTC_CODE_CAN_PASSIVE,
    DTC_CODE_CAN_BUS_OFF,
    DTC_CODE_NODE_B_WATCHDOG_RESET,
    DTC_CODE_COUNT
} DtcCode;

typedef enum
{
    DTC_TRANSITION_NONE = 0U,
    DTC_TRANSITION_ACTIVATED,
    DTC_TRANSITION_RECOVERED
} DtcTransition;

typedef struct
{
    uint8_t active;
    uint8_t stored;
    uint8_t freeze_frame_valid;
    uint8_t failure_count;
    uint8_t pass_count;
    uint16_t occurrence_count;
} DtcStatus;

typedef struct
{
    uint8_t health_flags;
    uint8_t can_last_error;
    uint8_t reset_cause;
    uint8_t distance_valid;
    uint16_t distance_mm;
    uint32_t local_timestamp_ms;
    uint32_t remote_uptime_ms;
} DtcFreezeFrame;

void DtcManager_Init(void);
DtcTransition DtcManager_UpdateCondition(DtcCode code,
                                         uint8_t condition_failed);
void DtcManager_RecordHistoricalEvent(DtcCode code);
void DtcManager_CaptureFreezeFrame(DtcCode code,
                                   const DtcFreezeFrame *freeze_frame);
void DtcManager_RestorePersistentState(
                DtcCode code,
                uint8_t stored,
                uint16_t occurrence_count,
                const DtcFreezeFrame *freeze_frame);
void DtcManager_ClearHistory(void);
uint8_t DtcManager_GetStatus(DtcCode code, DtcStatus *status);
uint8_t DtcManager_GetFreezeFrame(DtcCode code,
                                  DtcFreezeFrame *freeze_frame);
uint16_t DtcManager_GetActiveMask(void);
uint16_t DtcManager_GetStoredMask(void);
uint8_t DtcManager_GetActiveCount(void);
uint8_t DtcManager_GetStoredCount(void);

#endif

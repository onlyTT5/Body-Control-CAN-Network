#ifndef UI_H
#define UI_H

#include "body_control.h"
#include "bsp_button.h"
#include "bsp_reset.h"
#include "bsp_encoder.h"

typedef enum
{
    UI_DTC_CLEAR_RESULT_NONE = 0,
    UI_DTC_CLEAR_RESULT_PENDING,
    UI_DTC_CLEAR_RESULT_OK,
    UI_DTC_CLEAR_RESULT_ACTIVE_ERROR,
    UI_DTC_CLEAR_RESULT_FLASH_ERROR
} UiDtcClearResult;

void Ui_Init(void);
void Ui_ShowBootSelfTest(BspResetCause reset_cause);
void Ui_InitDashboard(void);
void Ui_ShowStatus(const BodyControlState *state);
void Ui_UpdateLight(const BodyControlState *state);
void Ui_UpdateHeartbeat(const BodyControlState *state);
void Ui_UpdateCanStatus(const BodyControlState *state);
void Ui_UpdateDtcStatus(uint8_t active_count, uint8_t stored_count);
void Ui_UpdateActuatorLight(const BodyControlState *state);
void Ui_UpdateParkDistance(uint8_t received,
                           uint8_t valid,
                           uint16_t distance_mm);
void Ui_HandleEncoderEvent(BspEncoderEvent event);
void Ui_HandleMenuButtonEvent(BspButtonEvent event);
uint8_t Ui_TakeDtcClearRequest(void);
void Ui_ShowDtcClearResult(UiDtcClearResult result);

#endif

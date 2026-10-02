#ifndef UI_H
#define UI_H

#include "body_control.h"

void Ui_Init(void);
void Ui_ShowBootSelfTest(void);
void Ui_InitDashboard(void);
void Ui_ShowStatus(const BodyControlState *state);
void Ui_UpdateLight(const BodyControlState *state);
void Ui_UpdateHeartbeat(const BodyControlState *state);
void Ui_UpdateCanStatus(const BodyControlState *state);
void Ui_UpdateActuatorLight(const BodyControlState *state);
void Ui_UpdateParkDistance(uint8_t received,
                           uint8_t valid,
                           uint16_t distance_mm);

#endif

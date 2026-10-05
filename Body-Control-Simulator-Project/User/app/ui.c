#include "ui.h"
#include "oled.h"
#include "dtc_manager.h"

#define UI_LINE_LENGTH 18U

typedef enum
{
    UI_PAGE_DASHBOARD = 0,
    UI_PAGE_DTC_SUMMARY,
    UI_PAGE_DTC_DETAIL,
    UI_PAGE_DTC_FREEZE,
    UI_PAGE_DTC_CLEAR,
    UI_PAGE_COUNT
} UiPage;

static UiPage s_current_page = UI_PAGE_DASHBOARD;
static DtcCode s_selected_dtc = DTC_CODE_ULTRASONIC_INVALID;

static uint8_t s_can_online = 0U;
static uint8_t s_dtc_active_count = 0U;
static uint8_t s_dtc_stored_count = 0U;
static uint8_t s_light_on = 0U;
static uint8_t s_actuator_reply_timeout = 0U;
static uint8_t s_actuator_light_valid = 0U;
static uint8_t s_actuator_light_on = 0U;
static uint8_t s_park_received = 0U;
static uint8_t s_park_valid = 0U;
static uint16_t s_park_distance_mm = 0U;
static uint8_t s_dtc_clear_request = 0U;
static UiDtcClearResult s_dtc_clear_result = UI_DTC_CLEAR_RESULT_NONE;

static const char *s_dtc_names[DTC_CODE_COUNT] =
{
    "ULTRASONIC",
    "NODE B OFFLINE",
    "CAN WARNING",
    "CAN PASSIVE",
    "CAN BUS OFF",
    "B WATCHDOG"
};

static void Ui_ClearLine(char *line)
{
    uint8_t i;

    for(i = 0U; i < UI_LINE_LENGTH; i++)
    {
        line[i] = ' ';
    }
    line[UI_LINE_LENGTH] = '\0';
}

static void Ui_LinePutText(char *line, uint8_t column, const char *text)
{
    while((*text != '\0') && (column < UI_LINE_LENGTH))
    {
        line[column] = *text;
        column++;
        text++;
    }
}

static void Ui_LinePutDecimal(char *line,
                              uint8_t column,
                              uint8_t width,
                              uint32_t value)
{
    uint8_t i;

    for(i = 0U; i < width; i++)
    {
        line[column + width - 1U - i] =
            (char)('0' + (value % 10U));
        value /= 10U;
    }
}

static char Ui_HexDigit(uint8_t value)
{
    value &= 0x0FU;
    if(value < 10U)
    {
        return (char)('0' + value);
    }

    return (char)('A' + (value - 10U));
}

static void Ui_LinePutHexByte(char *line, uint8_t column, uint8_t value)
{
    line[column] = Ui_HexDigit((uint8_t)(value >> 4));
    line[column + 1U] = Ui_HexDigit(value);
}

static void Ui_ShowLine(uint8_t page, const char *line)
{
    OLED_ShowString(page, 20, (char *)line);
}

static void Ui_DrawCanAndDtcStatus(void)
{
    char line[17];

    line[0] = 'C';
    line[1] = 'A';
    line[2] = 'N';
    line[3] = ':';
    line[4] = 'O';

    if(s_can_online != 0U)
    {
        line[5] = 'N';
        line[6] = ' ';
    }
    else
    {
        line[5] = 'F';
        line[6] = 'F';
    }

    line[7] = ' ';
    line[8] = 'D';
    line[9] = 'T';
    line[10] = 'C';
    line[11] = ':';
    line[12] = 'A';
    line[13] = (char)('0' + (s_dtc_active_count % 10U));
    line[14] = 'H';
    line[15] = (char)('0' + (s_dtc_stored_count % 10U));
    line[16] = '\0';

    OLED_ShowString(2, 4, line);
}

static void Ui_DrawParkDistance(void)
{
    char line[19];
    uint16_t distance_cm;
    uint8_t digit_index;

    if(s_park_received == 0U)
    {
        Ui_ShowLine(0, "PARK: WAIT        ");
        return;
    }

    if(s_park_valid == 0U)
    {
        Ui_ShowLine(0, "PARK: ERROR       ");
        return;
    }

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "PARK:");
    line[10] = '.';
    line[11] = (char)('0' + (s_park_distance_mm % 10U));
    line[12] = 'C';
    line[13] = 'M';

    distance_cm = s_park_distance_mm / 10U;
    digit_index = 9U;
    do
    {
        line[digit_index] = (char)('0' + (distance_cm % 10U));
        distance_cm /= 10U;
        digit_index--;
    }
    while((distance_cm != 0U) && (digit_index >= 6U));

    Ui_ShowLine(0, line);
}

static void Ui_DrawLight(void)
{
    if(s_light_on != 0U)
    {
        Ui_ShowLine(4, "LIGHTA: ON        ");
    }
    else
    {
        Ui_ShowLine(4, "LIGHTA: OFF       ");
    }
}

static void Ui_DrawActuatorLight(void)
{
    if(s_actuator_reply_timeout != 0U)
    {
        Ui_ShowLine(6, "LIGHTB: ERROR     ");
    }
    else if(s_actuator_light_valid == 0U)
    {
        Ui_ShowLine(6, "LIGHTB: WAIT      ");
    }
    else if(s_actuator_light_on != 0U)
    {
        Ui_ShowLine(6, "LIGHTB: ON        ");
    }
    else
    {
        Ui_ShowLine(6, "LIGHTB: OFF       ");
    }
}

static void Ui_DrawDashboard(void)
{
    OLED_Clear();
    Ui_DrawParkDistance();
    Ui_DrawCanAndDtcStatus();
    Ui_DrawLight();
    Ui_DrawActuatorLight();
}

static uint8_t Ui_DtcIsVisible(DtcCode code)
{
    DtcStatus status;

    if(DtcManager_GetStatus(code, &status) == 0U)
    {
        return 0U;
    }

    return ((status.active != 0U) || (status.stored != 0U)) ? 1U : 0U;
}

static uint8_t Ui_EnsureSelectedDtc(void)
{
    uint8_t i;

    if(Ui_DtcIsVisible(s_selected_dtc) != 0U)
    {
        return 1U;
    }

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        if(Ui_DtcIsVisible((DtcCode)i) != 0U)
        {
            s_selected_dtc = (DtcCode)i;
            return 1U;
        }
    }

    return 0U;
}

static void Ui_SelectNextDtc(void)
{
    uint8_t offset;
    uint8_t index;

    for(offset = 1U; offset <= (uint8_t)DTC_CODE_COUNT; offset++)
    {
        index = (uint8_t)(((uint8_t)s_selected_dtc + offset) %
                          (uint8_t)DTC_CODE_COUNT);
        if(Ui_DtcIsVisible((DtcCode)index) != 0U)
        {
            s_selected_dtc = (DtcCode)index;
            return;
        }
    }
}

static void Ui_DrawDtcSummary(void)
{
    char line[19];

    OLED_Clear();
    Ui_ShowLine(0, "DTC SUMMARY 2/5   ");

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "ACTIVE:");
    Ui_LinePutDecimal(line, 8U, 2U, s_dtc_active_count);
    Ui_ShowLine(2, line);

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "HISTORY:");
    Ui_LinePutDecimal(line, 9U, 2U, s_dtc_stored_count);
    Ui_ShowLine(4, line);

    Ui_ShowLine(6, "ROTATE: PAGE      ");
}

static void Ui_DrawDtcDetail(void)
{
    DtcStatus status;
    char line[19];
    uint8_t code_number;

    OLED_Clear();
    Ui_ShowLine(0, "DTC DETAIL 3/5    ");

    if((Ui_EnsureSelectedDtc() == 0U) ||
       (DtcManager_GetStatus(s_selected_dtc, &status) == 0U))
    {
        Ui_ShowLine(2, "NO ACTIVE/HISTORY ");
        Ui_ShowLine(6, "ROTATE: PAGE      ");
        return;
    }

    code_number = (uint8_t)s_selected_dtc + 1U;
    Ui_ClearLine(line);
    line[0] = 'D';
    line[1] = '0';
    line[2] = (char)('0' + code_number);
    line[3] = ' ';
    Ui_LinePutText(line, 4U, s_dtc_names[(uint8_t)s_selected_dtc]);
    Ui_ShowLine(2, line);

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "A:");
    line[2] = (status.active != 0U) ? '1' : '0';
    Ui_LinePutText(line, 4U, "H:");
    line[6] = (status.stored != 0U) ? '1' : '0';
    Ui_LinePutText(line, 8U, "C:");
    Ui_LinePutDecimal(line, 10U, 5U, status.occurrence_count);
    Ui_ShowLine(4, line);

    Ui_ShowLine(6, "KEY: NEXT DTC     ");
}

static void Ui_DrawDtcFreeze(void)
{
    DtcFreezeFrame freeze_frame;
    char line[19];
    uint8_t code_number;

    OLED_Clear();

    if(Ui_EnsureSelectedDtc() == 0U)
    {
        Ui_ShowLine(0, "FREEZE 4/5        ");
        Ui_ShowLine(2, "NO ACTIVE/HISTORY ");
        Ui_ShowLine(6, "ROTATE: PAGE      ");
        return;
    }

    code_number = (uint8_t)s_selected_dtc + 1U;
    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "FREEZE D0");
    line[9] = (char)('0' + code_number);
    Ui_LinePutText(line, 11U, "4/5");
    Ui_ShowLine(0, line);

    if(DtcManager_GetFreezeFrame(s_selected_dtc, &freeze_frame) == 0U)
    {
        Ui_ShowLine(2, "NO FREEZE FRAME   ");
        Ui_ShowLine(6, "KEY: NEXT DTC     ");
        return;
    }

    Ui_ClearLine(line);
    if(freeze_frame.distance_valid != 0U)
    {
        Ui_LinePutText(line, 0U, "DIST:");
        Ui_LinePutDecimal(line, 5U, 5U, freeze_frame.distance_mm);
        Ui_LinePutText(line, 10U, "MM");
    }
    else
    {
        Ui_LinePutText(line, 0U, "DIST:INVALID");
    }
    Ui_ShowLine(2, line);

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "H:");
    Ui_LinePutHexByte(line, 2U, freeze_frame.health_flags);
    Ui_LinePutText(line, 5U, "L:");
    Ui_LinePutHexByte(line, 7U, freeze_frame.can_last_error);
    Ui_LinePutText(line, 10U, "R:");
    Ui_LinePutHexByte(line, 12U, freeze_frame.reset_cause);
    Ui_ShowLine(4, line);

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "T:");
    Ui_LinePutDecimal(line, 2U, 5U,
                      freeze_frame.local_timestamp_ms / 1000U);
    Ui_LinePutText(line, 8U, "U:");
    Ui_LinePutDecimal(line, 10U, 5U,
                      freeze_frame.remote_uptime_ms / 1000U);
    Ui_ShowLine(6, line);
}

static void Ui_DrawDtcClear(void)
{
    char line[19];

    OLED_Clear();
    Ui_ShowLine(0, "CLEAR DTC 5/5     ");

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "ACTIVE:");
    Ui_LinePutDecimal(line, 8U, 2U, s_dtc_active_count);
    Ui_ShowLine(2, line);

    Ui_ClearLine(line);
    Ui_LinePutText(line, 0U, "HISTORY:");
    Ui_LinePutDecimal(line, 9U, 2U, s_dtc_stored_count);
    Ui_ShowLine(4, line);

    if(s_dtc_clear_result == UI_DTC_CLEAR_RESULT_PENDING)
    {
        Ui_ShowLine(6, "CLEARING...       ");
    }
    else if(s_dtc_clear_result == UI_DTC_CLEAR_RESULT_OK)
    {
        Ui_ShowLine(6, "CLEAR OK          ");
    }
    else if(s_dtc_clear_result == UI_DTC_CLEAR_RESULT_ACTIVE_ERROR)
    {
        Ui_ShowLine(6, "ACTIVE ERROR      ");
    }
    else if(s_dtc_clear_result == UI_DTC_CLEAR_RESULT_FLASH_ERROR)
    {
        Ui_ShowLine(6, "FLASH ERROR       ");
    }
    else
    {
        Ui_ShowLine(6, "HOLD 2S TO CLEAR  ");
    }
}

static void Ui_DrawCurrentPage(void)
{
    if(s_current_page == UI_PAGE_DASHBOARD)
    {
        Ui_DrawDashboard();
    }
    else if(s_current_page == UI_PAGE_DTC_SUMMARY)
    {
        Ui_DrawDtcSummary();
    }
    else if(s_current_page == UI_PAGE_DTC_DETAIL)
    {
        Ui_DrawDtcDetail();
    }
    else if(s_current_page == UI_PAGE_DTC_FREEZE)
    {
        Ui_DrawDtcFreeze();
    }
    else
    {
        Ui_DrawDtcClear();
    }
}

void Ui_Init(void)
{
    OLED_Init();
}

void Ui_ShowBootSelfTest(BspResetCause reset_cause)
{
    OLED_Clear();

    OLED_ShowString(0, 10, "BODY CONTROL V0.8");
    OLED_ShowString(2, 20, "OLED: ON");

    if(reset_cause == BSP_RESET_CAUSE_POWER)
    {
        OLED_ShowString(4, 20, "RST: POWER");
    }
    else if(reset_cause == BSP_RESET_CAUSE_PIN)
    {
        OLED_ShowString(4, 20, "RST: PIN  ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_SOFTWARE)
    {
        OLED_ShowString(4, 20, "RST: SOFT ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_IWDG)
    {
        OLED_ShowString(4, 20, "RST: IWDG ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_WWDG)
    {
        OLED_ShowString(4, 20, "RST: WWDG ");
    }
    else if(reset_cause == BSP_RESET_CAUSE_LOW_POWER)
    {
        OLED_ShowString(4, 20, "RST: LOWPWR");
    }
    else
    {
        OLED_ShowString(4, 20, "RST: OTHER");
    }

    OLED_ShowString(6, 20, "CAN: OFFLINE");
}

void Ui_InitDashboard(void)
{
    s_current_page = UI_PAGE_DASHBOARD;
    s_selected_dtc = DTC_CODE_ULTRASONIC_INVALID;
    s_can_online = 0U;
    s_dtc_active_count = 0U;
    s_dtc_stored_count = 0U;
    s_light_on = 0U;
    s_actuator_reply_timeout = 0U;
    s_actuator_light_valid = 0U;
    s_actuator_light_on = 0U;
    s_park_received = 0U;
    s_park_valid = 0U;
    s_park_distance_mm = 0U;
    s_dtc_clear_request = 0U;
    s_dtc_clear_result = UI_DTC_CLEAR_RESULT_NONE;
    Ui_DrawDashboard();
}

void Ui_UpdateLight(const BodyControlState *state)
{
    s_light_on = (state->light_on != 0U) ? 1U : 0U;
    if(s_current_page == UI_PAGE_DASHBOARD)
    {
        Ui_DrawLight();
    }
}

void Ui_UpdateHeartbeat(const BodyControlState *state)
{
    if(s_current_page != UI_PAGE_DASHBOARD)
    {
        return;
    }

    if(state->heartbeat_on != 0U)
    {
        Ui_ShowLine(6, "HB: ON            ");
    }
    else
    {
        Ui_ShowLine(6, "HB: OFF           ");
    }
}

void Ui_ShowStatus(const BodyControlState *state)
{
    Ui_UpdateCanStatus(state);
    Ui_UpdateLight(state);
    Ui_UpdateActuatorLight(state);
}

void Ui_UpdateCanStatus(const BodyControlState *state)
{
    s_can_online = (state->can_online != 0U) ? 1U : 0U;
    if(s_current_page == UI_PAGE_DASHBOARD)
    {
        Ui_DrawCanAndDtcStatus();
    }
}

void Ui_UpdateDtcStatus(uint8_t active_count, uint8_t stored_count)
{
    s_dtc_active_count = active_count;
    s_dtc_stored_count = stored_count;

    if(s_current_page == UI_PAGE_DASHBOARD)
    {
        Ui_DrawCanAndDtcStatus();
    }
    else
    {
        Ui_DrawCurrentPage();
    }
}

void Ui_UpdateActuatorLight(const BodyControlState *state)
{
    s_actuator_reply_timeout =
        (state->actuator_reply_timeout != 0U) ? 1U : 0U;
    s_actuator_light_valid =
        (state->actuator_light_valid != 0U) ? 1U : 0U;
    s_actuator_light_on =
        (state->actuator_light_on != 0U) ? 1U : 0U;

    if(s_current_page == UI_PAGE_DASHBOARD)
    {
        Ui_DrawActuatorLight();
    }
}

void Ui_UpdateParkDistance(uint8_t received,
                           uint8_t valid,
                           uint16_t distance_mm)
{
    s_park_received = (received != 0U) ? 1U : 0U;
    s_park_valid = (valid != 0U) ? 1U : 0U;
    s_park_distance_mm = distance_mm;

    if(s_current_page == UI_PAGE_DASHBOARD)
    {
        Ui_DrawParkDistance();
    }
}

void Ui_HandleEncoderEvent(BspEncoderEvent event)
{
    if(event == BSP_ENCODER_EVENT_CLOCKWISE)
    {
        if(s_current_page >= (UI_PAGE_COUNT - 1))
        {
            s_current_page = UI_PAGE_DASHBOARD;
        }
        else
        {
            s_current_page = (UiPage)((uint8_t)s_current_page + 1U);
        }
    }
    else if(event == BSP_ENCODER_EVENT_COUNTERCLOCKWISE)
    {
        if(s_current_page == UI_PAGE_DASHBOARD)
        {
            s_current_page = (UiPage)(UI_PAGE_COUNT - 1);
        }
        else
        {
            s_current_page = (UiPage)((uint8_t)s_current_page - 1U);
        }
    }
    else
    {
        return;
    }

    s_dtc_clear_result = UI_DTC_CLEAR_RESULT_NONE;
    Ui_DrawCurrentPage();
}

void Ui_HandleMenuButtonEvent(BspButtonEvent event)
{
    if(event == BSP_BUTTON_EVENT_LONG_PRESS)
    {
        if(s_current_page != UI_PAGE_DTC_CLEAR)
        {
            s_current_page = UI_PAGE_DASHBOARD;
            Ui_DrawCurrentPage();
        }
        return;
    }

    if(event == BSP_BUTTON_EVENT_HOLD_2S)
    {
        if(s_current_page == UI_PAGE_DTC_CLEAR)
        {
            s_dtc_clear_result = UI_DTC_CLEAR_RESULT_PENDING;
            s_dtc_clear_request = 1U;
            Ui_DrawDtcClear();
        }
        return;
    }

    if(event != BSP_BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    if((s_current_page == UI_PAGE_DTC_DETAIL) ||
       (s_current_page == UI_PAGE_DTC_FREEZE))
    {
        Ui_SelectNextDtc();
        Ui_DrawCurrentPage();
    }
}

uint8_t Ui_TakeDtcClearRequest(void)
{
    uint8_t request;

    request = s_dtc_clear_request;
    s_dtc_clear_request = 0U;
    return request;
}

void Ui_ShowDtcClearResult(UiDtcClearResult result)
{
    s_dtc_clear_result = result;

    if(s_current_page == UI_PAGE_DTC_CLEAR)
    {
        Ui_DrawDtcClear();
    }
}

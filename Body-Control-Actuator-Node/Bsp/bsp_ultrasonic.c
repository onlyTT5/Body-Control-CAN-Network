#include "bsp_ultrasonic.h"

#define BSP_ULTRASONIC_TRIGGER_PIN          GPIO_Pin_1
#define BSP_ULTRASONIC_TRIGGER_PORT         GPIOA
#define BSP_ULTRASONIC_ECHO_PIN             GPIO_Pin_0
#define BSP_ULTRASONIC_ECHO_PORT            GPIOA

#define BSP_ULTRASONIC_STATE_IDLE           0U
#define BSP_ULTRASONIC_STATE_WAIT_RISING    1U
#define BSP_ULTRASONIC_STATE_WAIT_FALLING   2U

#define BSP_ULTRASONIC_TRIGGER_PULSE_US     10U
#define BSP_ULTRASONIC_ECHO_TIMEOUT_MS      40U
#define BSP_ULTRASONIC_MIN_ECHO_US          100U
#define BSP_ULTRASONIC_MAX_ECHO_US          30000U
#define BSP_ULTRASONIC_MEDIAN_FILTER_SIZE   5U

static volatile uint8_t s_measurement_state = BSP_ULTRASONIC_STATE_IDLE;
static volatile uint8_t s_result_ready = 0U;
static volatile uint8_t s_result_valid = 0U;
static volatile uint16_t s_rising_capture = 0U;
static volatile uint16_t s_distance_mm = 0U;
static uint32_t s_measurement_start_tick = 0U;
static uint16_t s_distance_filter_samples[
                    BSP_ULTRASONIC_MEDIAN_FILTER_SIZE];
static uint8_t s_distance_filter_count = 0U;
static uint8_t s_distance_filter_write_index = 0U;

static uint16_t BspUltrasonic_FilterDistance(uint16_t distance_mm)
{
    uint16_t sorted_samples[BSP_ULTRASONIC_MEDIAN_FILTER_SIZE];
    uint16_t value;
    uint8_t i;
    uint8_t j;

    s_distance_filter_samples[s_distance_filter_write_index] = distance_mm;
    s_distance_filter_write_index++;

    if(s_distance_filter_write_index >=
       BSP_ULTRASONIC_MEDIAN_FILTER_SIZE)
    {
        s_distance_filter_write_index = 0U;
    }

    if(s_distance_filter_count < BSP_ULTRASONIC_MEDIAN_FILTER_SIZE)
    {
        s_distance_filter_count++;

        /* 窗口未填满时保持即时输出，避免启动后等待 5 次测量。 */
        if(s_distance_filter_count < BSP_ULTRASONIC_MEDIAN_FILTER_SIZE)
        {
            return distance_mm;
        }
    }

    for(i = 0U; i < BSP_ULTRASONIC_MEDIAN_FILTER_SIZE; i++)
    {
        sorted_samples[i] = s_distance_filter_samples[i];
    }

    /* 对 5 个元素执行插入排序，第三个元素就是中值。 */
    for(i = 1U; i < BSP_ULTRASONIC_MEDIAN_FILTER_SIZE; i++)
    {
        value = sorted_samples[i];
        j = i;

        while((j > 0U) && (sorted_samples[j - 1U] > value))
        {
            sorted_samples[j] = sorted_samples[j - 1U];
            j--;
        }

        sorted_samples[j] = value;
    }

    return sorted_samples[BSP_ULTRASONIC_MEDIAN_FILTER_SIZE / 2U];
}

static void BspUltrasonic_SetCapturePolarity(uint16_t polarity)
{
    TIM2->CCER &= (uint16_t)(~TIM_CCER_CC1P);
    TIM2->CCER |= polarity;
}

void BspUltrasonic_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    TIM_TimeBaseInitTypeDef timer_init;
    TIM_ICInitTypeDef input_capture_init;
    NVIC_InitTypeDef nvic_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO,
                           ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    gpio_init.GPIO_Pin = BSP_ULTRASONIC_TRIGGER_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BSP_ULTRASONIC_TRIGGER_PORT, &gpio_init);
    GPIO_ResetBits(BSP_ULTRASONIC_TRIGGER_PORT,
                   BSP_ULTRASONIC_TRIGGER_PIN);

    gpio_init.GPIO_Pin = BSP_ULTRASONIC_ECHO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(BSP_ULTRASONIC_ECHO_PORT, &gpio_init);

    TIM_TimeBaseStructInit(&timer_init);
    timer_init.TIM_Prescaler = 71U;
    timer_init.TIM_Period = 0xFFFFU;
    timer_init.TIM_CounterMode = TIM_CounterMode_Up;
    timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &timer_init);

    TIM_ICStructInit(&input_capture_init);
    input_capture_init.TIM_Channel = TIM_Channel_1;
    input_capture_init.TIM_ICPolarity = TIM_ICPolarity_Rising;
    input_capture_init.TIM_ICSelection = TIM_ICSelection_DirectTI;
    input_capture_init.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    input_capture_init.TIM_ICFilter = 8U;
    TIM_ICInit(TIM2, &input_capture_init);

    TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
    TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    nvic_init.NVIC_IRQChannel = TIM2_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic_init.NVIC_IRQChannelSubPriority = 0U;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);

    TIM_Cmd(TIM2, ENABLE);
}

uint8_t BspUltrasonic_StartMeasurement(uint32_t current_tick_ms)
{
    uint16_t start_counter;

    if((s_measurement_state != BSP_ULTRASONIC_STATE_IDLE) ||
       (s_result_ready != 0U))
    {
        return 0U;
    }

    BspUltrasonic_SetCapturePolarity(TIM_ICPolarity_Rising);
    TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);

    s_measurement_start_tick = current_tick_ms;
    s_measurement_state = BSP_ULTRASONIC_STATE_WAIT_RISING;

    GPIO_SetBits(BSP_ULTRASONIC_TRIGGER_PORT,
                 BSP_ULTRASONIC_TRIGGER_PIN);

    start_counter = TIM_GetCounter(TIM2);
    while((uint16_t)(TIM_GetCounter(TIM2) - start_counter) <
          BSP_ULTRASONIC_TRIGGER_PULSE_US)
    {
    }

    GPIO_ResetBits(BSP_ULTRASONIC_TRIGGER_PORT,
                   BSP_ULTRASONIC_TRIGGER_PIN);

    return 1U;
}

void BspUltrasonic_Task(uint32_t current_tick_ms)
{
    if((s_measurement_state != BSP_ULTRASONIC_STATE_IDLE) &&
       ((current_tick_ms - s_measurement_start_tick) >=
        BSP_ULTRASONIC_ECHO_TIMEOUT_MS))
    {
        NVIC_DisableIRQ(TIM2_IRQn);

        s_measurement_state = BSP_ULTRASONIC_STATE_IDLE;
        s_distance_mm = 0xFFFFU;
        s_result_valid = 0U;
        s_result_ready = 1U;

        BspUltrasonic_SetCapturePolarity(TIM_ICPolarity_Rising);
        TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);

        NVIC_EnableIRQ(TIM2_IRQn);
    }
}

uint8_t BspUltrasonic_GetResult(uint16_t *distance_mm,
                                uint8_t *valid)
{
    if((distance_mm == 0) || (valid == 0) || (s_result_ready == 0U))
    {
        return 0U;
    }

    *valid = s_result_valid;

    if(s_result_valid != 0U)
    {
        *distance_mm = BspUltrasonic_FilterDistance(s_distance_mm);
    }
    else
    {
        *distance_mm = s_distance_mm;
    }

    s_result_ready = 0U;

    return 1U;
}

void TIM2_IRQHandler(void)
{
    uint16_t capture;
    uint16_t echo_time_us;
    uint32_t calculated_distance_mm;

    if(TIM_GetITStatus(TIM2, TIM_IT_CC1) == RESET)
    {
        return;
    }

    capture = TIM_GetCapture1(TIM2);
    TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);

    if(s_measurement_state == BSP_ULTRASONIC_STATE_WAIT_RISING)
    {
        s_rising_capture = capture;
        s_measurement_state = BSP_ULTRASONIC_STATE_WAIT_FALLING;
        BspUltrasonic_SetCapturePolarity(TIM_ICPolarity_Falling);
    }
    else if(s_measurement_state == BSP_ULTRASONIC_STATE_WAIT_FALLING)
    {
        echo_time_us = (uint16_t)(capture - s_rising_capture);
        s_measurement_state = BSP_ULTRASONIC_STATE_IDLE;
        BspUltrasonic_SetCapturePolarity(TIM_ICPolarity_Rising);

        if((echo_time_us >= BSP_ULTRASONIC_MIN_ECHO_US) &&
           (echo_time_us <= BSP_ULTRASONIC_MAX_ECHO_US))
        {
            calculated_distance_mm =
                (((uint32_t)echo_time_us * 343U) + 1000U) / 2000U;
            s_distance_mm = (uint16_t)calculated_distance_mm;
            s_result_valid = 1U;
        }
        else
        {
            s_distance_mm = 0xFFFFU;
            s_result_valid = 0U;
        }

        s_result_ready = 1U;
    }
    else
    {
        BspUltrasonic_SetCapturePolarity(TIM_ICPolarity_Rising);
    }
}

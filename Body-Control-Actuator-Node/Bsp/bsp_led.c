#include "bsp_led.h"

void BspLed_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_13;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOC, &gpio_init);

    BspLed_Set(0U);
}

void BspLed_Set(uint8_t on)
{
    /* Blue Pill 板载 PC13：低电平点亮 */
    if (on != 0U)
    {
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
    }
    else
    {
        GPIO_SetBits(GPIOC, GPIO_Pin_13);
    }
}

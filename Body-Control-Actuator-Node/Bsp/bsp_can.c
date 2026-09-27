#include "bsp_can.h"

uint8_t BspCan_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    CAN_InitTypeDef can_init;

    /* 开启 GPIOB、AFIO 和 CAN1 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO,
                           ENABLE);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    /*
     * CAN1 第 1 次重映射：
     * PB8  -> CAN_RX
     * PB9  -> CAN_TX
     */
    GPIO_PinRemapConfig(GPIO_Remap1_CAN1, ENABLE);

    /* PB8：CAN_RX */
    gpio_init.GPIO_Pin = GPIO_Pin_8;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio_init);

    /* PB9：CAN_TX */
    gpio_init.GPIO_Pin = GPIO_Pin_9;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio_init);

    CAN_DeInit(CAN1);
    CAN_StructInit(&can_init);

    can_init.CAN_TTCM = DISABLE;
    can_init.CAN_ABOM = DISABLE;
    can_init.CAN_AWUM = DISABLE;
    can_init.CAN_NART = DISABLE;  /* 允许自动重发 */
    can_init.CAN_RFLM = DISABLE;
    can_init.CAN_TXFP = DISABLE;

    can_init.CAN_Mode = CAN_Mode_Normal;

    /*
     * PCLK1 = 36 MHz：
     * 36MHz / [4 × (1 + 13 + 4)] = 500 kbps
     */
    can_init.CAN_SJW = CAN_SJW_1tq;
    can_init.CAN_BS1 = CAN_BS1_13tq;
    can_init.CAN_BS2 = CAN_BS2_4tq;
    can_init.CAN_Prescaler = 4;

    if (CAN_Init(CAN1, &can_init) == CAN_InitStatus_Failed)
    {
        return 0U;
    }

    return 1U;
}

uint8_t BspCan_SendStdData(uint16_t std_id,
                           const uint8_t *data,
                           uint8_t dlc)
{
    CanTxMsg tx_msg;
    uint8_t mailbox;
    uint8_t i;
    uint32_t timeout;

    if ((std_id > 0x7FFU) || (dlc > 8U) || (data == 0))
    {
        return 0U;
    }

    tx_msg.StdId = std_id;
    tx_msg.ExtId = 0U;
    tx_msg.IDE = CAN_Id_Standard;
    tx_msg.RTR = CAN_RTR_Data;
    tx_msg.DLC = dlc;

    for (i = 0U; i < dlc; i++)
    {
        tx_msg.Data[i] = data[i];
    }

    mailbox = CAN_Transmit(CAN1, &tx_msg);

    if (mailbox == CAN_TxStatus_NoMailBox)
    {
        return 0U;
    }

    timeout = 0U;

    while (CAN_TransmitStatus(CAN1, mailbox) == CAN_TxStatus_Pending)
    {
        timeout++;

        if (timeout > 1000000U)
        {
            return 0U;
        }
    }

    if (CAN_TransmitStatus(CAN1, mailbox) == CAN_TxStatus_Ok)
    {
        return 1U;
    }

    return 0U;
}

uint8_t BspCan_ReceiveStdData(uint16_t *std_id,
                              uint8_t *data,
                              uint8_t *dlc)
{
    CanRxMsg rx_msg;
    uint8_t i;

    if ((std_id == 0) || (data == 0) || (dlc == 0))
    {
        return 0U;
    }

    if (CAN_MessagePending(CAN1, CAN_FIFO0) == 0U)
    {
        return 0U;
    }

    CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);

    /* 当前只接受标准数据帧 */
    if ((rx_msg.IDE != CAN_Id_Standard) ||
        (rx_msg.RTR != CAN_RTR_Data))
    {
        return 0U;
    }

    *std_id = (uint16_t)rx_msg.StdId;
    *dlc = rx_msg.DLC;

    for (i = 0U; i < rx_msg.DLC; i++)
    {
        data[i] = rx_msg.Data[i];
    }

    return 1U;
}

uint8_t BspCan_SetRxStdDataFilter(uint16_t std_id)
{
    CAN_FilterInitTypeDef filter_init;

    if (std_id > 0x7FFU)
    {
        return 0U;
    }

    filter_init.CAN_FilterNumber = 0U;
    filter_init.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter_init.CAN_FilterScale = CAN_FilterScale_32bit;

    /*
     * 标准帧 11 位 ID 在 bxCAN 过滤器高 16 位中左移 5 位。
     * 0x100 << 5 = 0x2000。
     */
    filter_init.CAN_FilterIdHigh = (uint16_t)(std_id << 5);
    filter_init.CAN_FilterIdLow = 0x0000U;

    /*
     * 高 16 位掩码 0xFFE0：比较全部 11 位标准 ID。
     * 低 16 位掩码 0x0006：
     * bit2 比较 IDE=0（标准帧）
     * bit1 比较 RTR=0（数据帧）
     */
    filter_init.CAN_FilterMaskIdHigh = 0xFFE0U;
    filter_init.CAN_FilterMaskIdLow = 0x0006U;

    filter_init.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter_init.CAN_FilterActivation = ENABLE;

    CAN_FilterInit(&filter_init);

    return 1U;
}

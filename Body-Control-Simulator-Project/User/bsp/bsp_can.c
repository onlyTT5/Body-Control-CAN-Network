#include "bsp_can.h"

HAL_StatusTypeDef BspCan_Init(uint16_t filter_id0,
                              uint16_t filter_id1,
							  uint16_t filter_id2,
                              uint16_t filter_id3,
                              uint16_t filter_id4)
{
    CAN_FilterTypeDef filter_config;

    if ((filter_id0 > 0x7FFU) ||
    (filter_id1 > 0x7FFU) ||
    (filter_id2 > 0x7FFU) ||
    (filter_id3 > 0x7FFU) ||
    (filter_id4 > 0x7FFU))
    {
        return HAL_ERROR;
    }

    /*
     * 16 位列表模式共有四个匹配槽位。
     * 当前四个槽位接收灯控、心跳、状态回报和日志查询。
     */
    filter_config.FilterIdHigh =
    (uint16_t)(filter_id0 << 5);      /* 0x100 */
	filter_config.FilterIdLow =
		(uint16_t)(filter_id1 << 5);      /* 0x101 */
	filter_config.FilterMaskIdHigh =
		(uint16_t)(filter_id2 << 5);      /* 0x102 */
	filter_config.FilterMaskIdLow =
		(uint16_t)(filter_id3 << 5);      /* 0x103 */

    filter_config.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter_config.FilterBank = 0U;
    filter_config.FilterMode = CAN_FILTERMODE_IDLIST;
    filter_config.FilterScale = CAN_FILTERSCALE_16BIT;
    filter_config.FilterActivation = ENABLE;
    filter_config.SlaveStartFilterBank = 14U;

    if (HAL_CAN_ConfigFilter(&hcan, &filter_config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 第二个过滤器组的四个槽位都接收超声波距离帧。 */
    filter_config.FilterIdHigh = (uint16_t)(filter_id4 << 5);
    filter_config.FilterIdLow = (uint16_t)(filter_id4 << 5);
    filter_config.FilterMaskIdHigh = (uint16_t)(filter_id4 << 5);
    filter_config.FilterMaskIdLow = (uint16_t)(filter_id4 << 5);
    filter_config.FilterBank = 1U;

    if (HAL_CAN_ConfigFilter(&hcan, &filter_config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BspCan_SendStdData(uint16_t standard_id,
                                     const uint8_t *data,
                                     uint8_t dlc)
{
    CAN_TxHeaderTypeDef tx_header;
    uint32_t mailbox;
    uint8_t tx_data[BSP_CAN_MAX_DLC];
    uint8_t i;

    if ((data == NULL) || (dlc > BSP_CAN_MAX_DLC))
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < dlc; i++)
    {
        tx_data[i] = data[i];
    }

    tx_header.StdId = standard_id;
    tx_header.ExtId = 0U;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = dlc;
    tx_header.TransmitGlobalTime = DISABLE;

    return HAL_CAN_AddTxMessage(&hcan, &tx_header, tx_data, &mailbox);
}

uint8_t BspCan_ReceiveStdData(uint16_t *standard_id,
                              uint8_t *data,
                              uint8_t *dlc)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[BSP_CAN_MAX_DLC];
    uint8_t i;

    if ((standard_id == NULL) || (data == NULL) || (dlc == NULL))
    {
        return 0U;
    }

    if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0U)
    {
        return 0U;
    }

    if (HAL_CAN_GetRxMessage(&hcan,
                             CAN_RX_FIFO0,
                             &rx_header,
                             rx_data) != HAL_OK)
    {
        return 0U;
    }

    /* 当前项目只处理 11 位标准数据帧 */
    if ((rx_header.IDE != CAN_ID_STD) ||
        (rx_header.RTR != CAN_RTR_DATA) ||
        (rx_header.DLC > BSP_CAN_MAX_DLC))
    {
        return 0U;
    }

    *standard_id = (uint16_t)rx_header.StdId;
    *dlc = (uint8_t)rx_header.DLC;

    for (i = 0U; i < *dlc; i++)
    {
        data[i] = rx_data[i];
    }

    return 1U;
}

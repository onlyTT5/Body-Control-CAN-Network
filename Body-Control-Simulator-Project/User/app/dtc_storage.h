#ifndef DTC_STORAGE_H
#define DTC_STORAGE_H

#include "stm32f1xx_hal.h"

/* 扫描最新有效记录，并把历史 DTC 与冻结帧恢复到 DTC 管理器。 */
HAL_StatusTypeDef DtcStorage_Init(void);

/* 将当前历史 DTC 状态以追加记录的方式写入 W25Q128。 */
HAL_StatusTypeDef DtcStorage_Save(void);

/* 擦除独立 DTC 扇区，并重新从空记录区开始写入。 */
HAL_StatusTypeDef DtcStorage_Clear(void);

#endif

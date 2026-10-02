#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "stm32f1xx_hal.h"

#define BSP_FLASH_MANUFACTURER_ID    0xEFU
#define BSP_FLASH_MEMORY_TYPE        0x40U
#define BSP_FLASH_CAPACITY_ID        0x18U

#define BSP_FLASH_SIZE_BYTES         0x01000000UL
#define BSP_FLASH_PAGE_SIZE          256U
#define BSP_FLASH_SECTOR_SIZE        4096U

HAL_StatusTypeDef BspFlash_ReadJedecId(uint8_t jedec_id[3]);
uint8_t BspFlash_IsW25Q128(const uint8_t jedec_id[3]);
HAL_StatusTypeDef BspFlash_Read(uint32_t address,
                                uint8_t *data,
                                uint16_t length);
HAL_StatusTypeDef BspFlash_ProgramPage(uint32_t address,
                                       const uint8_t *data,
                                       uint16_t length);
HAL_StatusTypeDef BspFlash_EraseSector4K(uint32_t address);

#endif

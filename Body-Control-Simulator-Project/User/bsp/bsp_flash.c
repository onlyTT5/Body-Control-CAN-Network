#include "bsp_flash.h"
#include "main.h"
#include "spi.h"

#define W25Q128_CMD_READ_JEDEC_ID    0x9FU
#define W25Q128_CMD_READ_DATA        0x03U
#define W25Q128_CMD_PAGE_PROGRAM     0x02U
#define W25Q128_CMD_SECTOR_ERASE_4K  0x20U
#define W25Q128_CMD_WRITE_ENABLE     0x06U
#define W25Q128_CMD_READ_STATUS_1    0x05U

#define W25Q128_STATUS_BUSY_MASK     0x01U
#define W25Q128_STATUS_WEL_MASK      0x02U
#define W25Q128_SPI_TIMEOUT_MS       100U
#define W25Q128_PROGRAM_TIMEOUT_MS   20U
#define W25Q128_ERASE_TIMEOUT_MS     2000U

static void BspFlash_Select(void)
{
    HAL_GPIO_WritePin(
        FLASH_CS_GPIO_Port,
        FLASH_CS_Pin,
        GPIO_PIN_RESET
    );
}

static void BspFlash_Unselect(void)
{
    HAL_GPIO_WritePin(
        FLASH_CS_GPIO_Port,
        FLASH_CS_Pin,
        GPIO_PIN_SET
    );
}

static HAL_StatusTypeDef BspFlash_ReadStatus1(uint8_t *status_register)
{
    uint8_t command;
    HAL_StatusTypeDef status;

    if(status_register == 0)
    {
        return HAL_ERROR;
    }

    command = W25Q128_CMD_READ_STATUS_1;
    BspFlash_Select();

    status = HAL_SPI_Transmit(&hspi1,
                              &command,
                              1U,
                              W25Q128_SPI_TIMEOUT_MS);

    if(status == HAL_OK)
    {
        status = HAL_SPI_Receive(&hspi1,
                                 status_register,
                                 1U,
                                 W25Q128_SPI_TIMEOUT_MS);
    }

    BspFlash_Unselect();
    return status;
}

static HAL_StatusTypeDef BspFlash_WaitWhileBusy(uint32_t timeout_ms)
{
    uint8_t status_register;
    uint32_t start_tick;

    start_tick = HAL_GetTick();

    do
    {
        if(BspFlash_ReadStatus1(&status_register) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if((status_register & W25Q128_STATUS_BUSY_MASK) == 0U)
        {
            return HAL_OK;
        }
    }
    while((HAL_GetTick() - start_tick) < timeout_ms);

    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef BspFlash_WriteEnable(void)
{
    uint8_t command;
    uint8_t status_register;
    HAL_StatusTypeDef status;

    command = W25Q128_CMD_WRITE_ENABLE;
    BspFlash_Select();
    status = HAL_SPI_Transmit(&hspi1,
                              &command,
                              1U,
                              W25Q128_SPI_TIMEOUT_MS);
    BspFlash_Unselect();

    if(status != HAL_OK)
    {
        return status;
    }

    status = BspFlash_ReadStatus1(&status_register);
    if(status != HAL_OK)
    {
        return status;
    }

    if((status_register & W25Q128_STATUS_WEL_MASK) == 0U)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static void BspFlash_BuildAddressHeader(uint8_t command,
                                        uint32_t address,
                                        uint8_t header[4])
{
    header[0] = command;
    header[1] = (uint8_t)(address >> 16);
    header[2] = (uint8_t)(address >> 8);
    header[3] = (uint8_t)address;
}

HAL_StatusTypeDef BspFlash_ReadJedecId(uint8_t jedec_id[3])
{
    uint8_t tx_data[4] =
    {
        W25Q128_CMD_READ_JEDEC_ID,
        0xFFU,
        0xFFU,
        0xFFU
    };

    uint8_t rx_data[4] =
    {
        0U,
        0U,
        0U,
        0U
    };

    HAL_StatusTypeDef status;

    if (jedec_id == 0)
    {
        return HAL_ERROR;
    }

    BspFlash_Select();

    status = HAL_SPI_TransmitReceive(
        &hspi1,
        tx_data,
        rx_data,
        4U,
        W25Q128_SPI_TIMEOUT_MS
    );

    BspFlash_Unselect();

    if (status != HAL_OK)
    {
        return status;
    }

    jedec_id[0] = rx_data[1];
    jedec_id[1] = rx_data[2];
    jedec_id[2] = rx_data[3];

    return HAL_OK;
}

uint8_t BspFlash_IsW25Q128(const uint8_t jedec_id[3])
{
    if (jedec_id == 0)
    {
        return 0U;
    }

    if ((jedec_id[0] == BSP_FLASH_MANUFACTURER_ID) &&
        (jedec_id[1] == BSP_FLASH_MEMORY_TYPE) &&
        (jedec_id[2] == BSP_FLASH_CAPACITY_ID))
    {
        return 1U;
    }

    return 0U;
}

HAL_StatusTypeDef BspFlash_Read(uint32_t address,
                                uint8_t *data,
                                uint16_t length)
{
    uint8_t header[4];
    HAL_StatusTypeDef status;

    if((data == 0) || (length == 0U) ||
       (address >= BSP_FLASH_SIZE_BYTES) ||
       ((uint32_t)length > (BSP_FLASH_SIZE_BYTES - address)))
    {
        return HAL_ERROR;
    }

    BspFlash_BuildAddressHeader(W25Q128_CMD_READ_DATA,
                                address,
                                header);

    BspFlash_Select();
    status = HAL_SPI_Transmit(&hspi1,
                              header,
                              4U,
                              W25Q128_SPI_TIMEOUT_MS);

    if(status == HAL_OK)
    {
        status = HAL_SPI_Receive(&hspi1,
                                 data,
                                 length,
                                 W25Q128_SPI_TIMEOUT_MS);
    }

    BspFlash_Unselect();
    return status;
}

HAL_StatusTypeDef BspFlash_ProgramPage(uint32_t address,
                                       const uint8_t *data,
                                       uint16_t length)
{
    uint8_t header[4];
    HAL_StatusTypeDef status;

    if((data == 0) || (length == 0U) ||
       (length > BSP_FLASH_PAGE_SIZE) ||
       (address >= BSP_FLASH_SIZE_BYTES) ||
       ((uint32_t)length > (BSP_FLASH_SIZE_BYTES - address)) ||
       (((address & (BSP_FLASH_PAGE_SIZE - 1U)) + length) >
        BSP_FLASH_PAGE_SIZE))
    {
        return HAL_ERROR;
    }

    status = BspFlash_WriteEnable();
    if(status != HAL_OK)
    {
        return status;
    }

    BspFlash_BuildAddressHeader(W25Q128_CMD_PAGE_PROGRAM,
                                address,
                                header);

    BspFlash_Select();
    status = HAL_SPI_Transmit(&hspi1,
                              header,
                              4U,
                              W25Q128_SPI_TIMEOUT_MS);

    if(status == HAL_OK)
    {
        status = HAL_SPI_Transmit(&hspi1,
                                  (uint8_t *)data,
                                  length,
                                  W25Q128_SPI_TIMEOUT_MS);
    }

    BspFlash_Unselect();

    if(status != HAL_OK)
    {
        return status;
    }

    return BspFlash_WaitWhileBusy(W25Q128_PROGRAM_TIMEOUT_MS);
}

HAL_StatusTypeDef BspFlash_EraseSector4K(uint32_t address)
{
    uint8_t header[4];
    HAL_StatusTypeDef status;

    if(address >= BSP_FLASH_SIZE_BYTES)
    {
        return HAL_ERROR;
    }

    address &= ~(BSP_FLASH_SECTOR_SIZE - 1UL);

    status = BspFlash_WriteEnable();
    if(status != HAL_OK)
    {
        return status;
    }

    BspFlash_BuildAddressHeader(W25Q128_CMD_SECTOR_ERASE_4K,
                                address,
                                header);

    BspFlash_Select();
    status = HAL_SPI_Transmit(&hspi1,
                              header,
                              4U,
                              W25Q128_SPI_TIMEOUT_MS);
    BspFlash_Unselect();

    if(status != HAL_OK)
    {
        return status;
    }

    return BspFlash_WaitWhileBusy(W25Q128_ERASE_TIMEOUT_MS);
}

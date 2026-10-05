#include "dtc_storage.h"

#include "bsp_flash.h"
#include "dtc_manager.h"

#define DTC_STORAGE_SECTOR_ADDRESS       0x00FFD000UL
#define DTC_STORAGE_SECTOR_END_ADDRESS   0x00FFE000UL
#define DTC_STORAGE_RECORD_SIZE          128U
#define DTC_STORAGE_RECORD_VERSION       1U
#define DTC_STORAGE_MAGIC_0              0x44U
#define DTC_STORAGE_MAGIC_1              0x54U
#define DTC_STORAGE_HEADER_SIZE          8U
#define DTC_STORAGE_ENTRY_SIZE           18U
#define DTC_STORAGE_CRC_OFFSET           126U

#define DTC_STORAGE_FLAG_STORED          0x01U
#define DTC_STORAGE_FLAG_FREEZE_VALID    0x02U

static uint32_t s_next_address = DTC_STORAGE_SECTOR_ADDRESS;
static uint32_t s_next_sequence = 0U;
static uint8_t s_initialized = 0U;

static uint16_t DtcStorage_CalculateCrc(const uint8_t *data,
                                        uint16_t length)
{
    uint16_t crc;
    uint16_t byte_index;
    uint8_t bit_index;

    crc = 0xFFFFU;

    for(byte_index = 0U; byte_index < length; byte_index++)
    {
        crc ^= (uint16_t)data[byte_index] << 8;

        for(bit_index = 0U; bit_index < 8U; bit_index++)
        {
            if((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static uint8_t DtcStorage_RecordIsErased(
                const uint8_t record[DTC_STORAGE_RECORD_SIZE])
{
    uint16_t i;

    for(i = 0U; i < DTC_STORAGE_RECORD_SIZE; i++)
    {
        if(record[i] != 0xFFU)
        {
            return 0U;
        }
    }

    return 1U;
}

static uint16_t DtcStorage_ReadUint16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t DtcStorage_ReadUint32(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void DtcStorage_WriteUint16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void DtcStorage_WriteUint32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static uint8_t DtcStorage_RecordIsValid(
                const uint8_t record[DTC_STORAGE_RECORD_SIZE])
{
    uint16_t stored_crc;
    uint16_t calculated_crc;

    if((record[0] != DTC_STORAGE_MAGIC_0) ||
       (record[1] != DTC_STORAGE_MAGIC_1) ||
       (record[2] != DTC_STORAGE_RECORD_VERSION) ||
       (record[3] != (uint8_t)DTC_CODE_COUNT))
    {
        return 0U;
    }

    stored_crc = DtcStorage_ReadUint16(&record[DTC_STORAGE_CRC_OFFSET]);
    calculated_crc = DtcStorage_CalculateCrc(
                                      record,
                                      DTC_STORAGE_CRC_OFFSET);

    return (stored_crc == calculated_crc) ? 1U : 0U;
}

static void DtcStorage_RestoreRecord(
                const uint8_t record[DTC_STORAGE_RECORD_SIZE])
{
    DtcFreezeFrame freeze_frame;
    const uint8_t *entry;
    const DtcFreezeFrame *freeze_frame_pointer;
    uint8_t flags;
    uint8_t i;

    for(i = 0U; i < (uint8_t)DTC_CODE_COUNT; i++)
    {
        entry = &record[DTC_STORAGE_HEADER_SIZE +
                        ((uint16_t)i * DTC_STORAGE_ENTRY_SIZE)];
        flags = entry[0];
        freeze_frame_pointer = 0;

        if((flags & DTC_STORAGE_FLAG_FREEZE_VALID) != 0U)
        {
            freeze_frame.health_flags = entry[3];
            freeze_frame.can_last_error = entry[4];
            freeze_frame.reset_cause = entry[5];
            freeze_frame.distance_valid = entry[6];
            freeze_frame.distance_mm = DtcStorage_ReadUint16(&entry[7]);
            freeze_frame.local_timestamp_ms =
                DtcStorage_ReadUint32(&entry[9]);
            freeze_frame.remote_uptime_ms =
                DtcStorage_ReadUint32(&entry[13]);
            freeze_frame_pointer = &freeze_frame;
        }

        DtcManager_RestorePersistentState(
            (DtcCode)i,
            ((flags & DTC_STORAGE_FLAG_STORED) != 0U) ? 1U : 0U,
            DtcStorage_ReadUint16(&entry[1]),
            freeze_frame_pointer);
    }
}

static void DtcStorage_BuildRecord(
                uint8_t record[DTC_STORAGE_RECORD_SIZE])
{
    DtcStatus status;
    DtcFreezeFrame freeze_frame;
    uint8_t *entry;
    uint16_t crc;
    uint16_t i;

    for(i = 0U; i < DTC_STORAGE_RECORD_SIZE; i++)
    {
        record[i] = 0xFFU;
    }

    record[0] = DTC_STORAGE_MAGIC_0;
    record[1] = DTC_STORAGE_MAGIC_1;
    record[2] = DTC_STORAGE_RECORD_VERSION;
    record[3] = (uint8_t)DTC_CODE_COUNT;
    DtcStorage_WriteUint32(&record[4], s_next_sequence);

    for(i = 0U; i < (uint16_t)DTC_CODE_COUNT; i++)
    {
        entry = &record[DTC_STORAGE_HEADER_SIZE +
                        (i * DTC_STORAGE_ENTRY_SIZE)];
        entry[0] = 0U;

        if(DtcManager_GetStatus((DtcCode)i, &status) != 0U)
        {
            if(status.stored != 0U)
            {
                entry[0] |= DTC_STORAGE_FLAG_STORED;
            }

            DtcStorage_WriteUint16(&entry[1], status.occurrence_count);
        }
        else
        {
            DtcStorage_WriteUint16(&entry[1], 0U);
        }

        if(DtcManager_GetFreezeFrame((DtcCode)i, &freeze_frame) != 0U)
        {
            entry[0] |= DTC_STORAGE_FLAG_FREEZE_VALID;
            entry[3] = freeze_frame.health_flags;
            entry[4] = freeze_frame.can_last_error;
            entry[5] = freeze_frame.reset_cause;
            entry[6] = freeze_frame.distance_valid;
            DtcStorage_WriteUint16(&entry[7], freeze_frame.distance_mm);
            DtcStorage_WriteUint32(&entry[9],
                                   freeze_frame.local_timestamp_ms);
            DtcStorage_WriteUint32(&entry[13],
                                   freeze_frame.remote_uptime_ms);
        }
        else
        {
            entry[3] = 0U;
            entry[4] = 0U;
            entry[5] = 0U;
            entry[6] = 0U;
            DtcStorage_WriteUint16(&entry[7], 0xFFFFU);
            DtcStorage_WriteUint32(&entry[9], 0U);
            DtcStorage_WriteUint32(&entry[13], 0U);
        }

        entry[17] = 0U;
    }

    crc = DtcStorage_CalculateCrc(record, DTC_STORAGE_CRC_OFFSET);
    DtcStorage_WriteUint16(&record[DTC_STORAGE_CRC_OFFSET], crc);
}

HAL_StatusTypeDef DtcStorage_Init(void)
{
    uint8_t record[DTC_STORAGE_RECORD_SIZE];
    uint8_t latest_record[DTC_STORAGE_RECORD_SIZE];
    uint8_t free_address_found;
    uint8_t sequence_found;
    uint16_t i;
    uint32_t address;
    uint32_t sequence;
    uint32_t highest_sequence;

    s_next_address = DTC_STORAGE_SECTOR_END_ADDRESS;
    s_next_sequence = 0U;
    s_initialized = 0U;
    free_address_found = 0U;
    sequence_found = 0U;
    highest_sequence = 0U;

    for(address = DTC_STORAGE_SECTOR_ADDRESS;
        address < DTC_STORAGE_SECTOR_END_ADDRESS;
        address += DTC_STORAGE_RECORD_SIZE)
    {
        if(BspFlash_Read(address,
                        record,
                        DTC_STORAGE_RECORD_SIZE) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if(DtcStorage_RecordIsErased(record) != 0U)
        {
            if(free_address_found == 0U)
            {
                s_next_address = address;
                free_address_found = 1U;
            }
        }
        else if(DtcStorage_RecordIsValid(record) != 0U)
        {
            sequence = DtcStorage_ReadUint32(&record[4]);

            if((sequence_found == 0U) ||
               ((int32_t)(sequence - highest_sequence) > 0))
            {
                highest_sequence = sequence;
                sequence_found = 1U;

                for(i = 0U; i < DTC_STORAGE_RECORD_SIZE; i++)
                {
                    latest_record[i] = record[i];
                }
            }
        }
    }

    if(sequence_found != 0U)
    {
        DtcStorage_RestoreRecord(latest_record);
        s_next_sequence = highest_sequence + 1U;
    }

    s_initialized = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef DtcStorage_Save(void)
{
    uint8_t record[DTC_STORAGE_RECORD_SIZE];
    uint8_t verify_record[DTC_STORAGE_RECORD_SIZE];
    uint16_t i;
    HAL_StatusTypeDef status;

    if(s_initialized == 0U)
    {
        return HAL_ERROR;
    }

    if(s_next_address >= DTC_STORAGE_SECTOR_END_ADDRESS)
    {
        status = BspFlash_EraseSector4K(DTC_STORAGE_SECTOR_ADDRESS);
        if(status != HAL_OK)
        {
            return status;
        }

        s_next_address = DTC_STORAGE_SECTOR_ADDRESS;
    }

    DtcStorage_BuildRecord(record);

    status = BspFlash_ProgramPage(s_next_address,
                                  record,
                                  DTC_STORAGE_RECORD_SIZE);
    if(status != HAL_OK)
    {
        return status;
    }

    status = BspFlash_Read(s_next_address,
                           verify_record,
                           DTC_STORAGE_RECORD_SIZE);
    if(status != HAL_OK)
    {
        return status;
    }

    for(i = 0U; i < DTC_STORAGE_RECORD_SIZE; i++)
    {
        if(verify_record[i] != record[i])
        {
            return HAL_ERROR;
        }
    }

    s_next_address += DTC_STORAGE_RECORD_SIZE;
    s_next_sequence++;
    return HAL_OK;
}

HAL_StatusTypeDef DtcStorage_Clear(void)
{
    HAL_StatusTypeDef status;

    if(s_initialized == 0U)
    {
        return HAL_ERROR;
    }

    status = BspFlash_EraseSector4K(DTC_STORAGE_SECTOR_ADDRESS);
    if(status != HAL_OK)
    {
        return status;
    }

    s_next_address = DTC_STORAGE_SECTOR_ADDRESS;
    s_next_sequence = 0U;
    return HAL_OK;
}

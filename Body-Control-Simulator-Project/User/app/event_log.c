#include "event_log.h"
#include "bsp_flash.h"

#define EVENT_LOG_SECTOR_ADDRESS       0x00FFE000UL
#define EVENT_LOG_SECTOR_END_ADDRESS   0x00FFF000UL
#define EVENT_LOG_RECORD_SIZE          16U
#define EVENT_LOG_RECORD_VERSION       1U
#define EVENT_LOG_MAGIC_0              0x42U
#define EVENT_LOG_MAGIC_1              0x43U

static uint32_t s_next_address = EVENT_LOG_SECTOR_ADDRESS;
static uint32_t s_next_sequence = 0U;
static uint16_t s_stored_count = 0U;
static uint8_t s_initialized = 0U;

static uint16_t EventLog_CalculateCrc(const uint8_t *data, uint8_t length)
{
    uint16_t crc;
    uint8_t byte_index;
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

static uint8_t EventLog_RecordIsErased(const uint8_t record[EVENT_LOG_RECORD_SIZE])
{
    uint8_t i;

    for(i = 0U; i < EVENT_LOG_RECORD_SIZE; i++)
    {
        if(record[i] != 0xFFU)
        {
            return 0U;
        }
    }

    return 1U;
}

static uint32_t EventLog_ReadUint32(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void EventLog_WriteUint32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static uint8_t EventLog_RecordIsValid(const uint8_t record[EVENT_LOG_RECORD_SIZE])
{
    uint16_t stored_crc;
    uint16_t calculated_crc;

    if((record[0] != EVENT_LOG_MAGIC_0) ||
       (record[1] != EVENT_LOG_MAGIC_1) ||
       (record[2] != EVENT_LOG_RECORD_VERSION))
    {
        return 0U;
    }

    stored_crc = (uint16_t)record[14] |
                 ((uint16_t)record[15] << 8);
    calculated_crc = EventLog_CalculateCrc(record, 14U);

    return (stored_crc == calculated_crc) ? 1U : 0U;
}

static void EventLog_BuildRecord(uint8_t record[EVENT_LOG_RECORD_SIZE],
                                 EventLogType type,
                                 uint8_t data0,
                                 uint8_t data1)
{
    uint16_t crc;

    record[0] = EVENT_LOG_MAGIC_0;
    record[1] = EVENT_LOG_MAGIC_1;
    record[2] = EVENT_LOG_RECORD_VERSION;
    record[3] = (uint8_t)type;
    EventLog_WriteUint32(&record[4], s_next_sequence);
    EventLog_WriteUint32(&record[8], HAL_GetTick());
    record[12] = data0;
    record[13] = data1;

    crc = EventLog_CalculateCrc(record, 14U);
    record[14] = (uint8_t)crc;
    record[15] = (uint8_t)(crc >> 8);
}

HAL_StatusTypeDef EventLog_Init(void)
{
    uint8_t record[EVENT_LOG_RECORD_SIZE];
    uint8_t free_address_found;
    uint8_t sequence_found;
    uint32_t address;
    uint32_t sequence;
    uint32_t highest_sequence;

    s_next_address = EVENT_LOG_SECTOR_END_ADDRESS;
    s_next_sequence = 0U;
    s_stored_count = 0U;
    s_initialized = 0U;
    free_address_found = 0U;
    sequence_found = 0U;
    highest_sequence = 0U;

    for(address = EVENT_LOG_SECTOR_ADDRESS;
        address < EVENT_LOG_SECTOR_END_ADDRESS;
        address += EVENT_LOG_RECORD_SIZE)
    {
        if(BspFlash_Read(address, record, EVENT_LOG_RECORD_SIZE) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if(EventLog_RecordIsErased(record) != 0U)
        {
            if(free_address_found == 0U)
            {
                s_next_address = address;
                free_address_found = 1U;
            }
        }
        else if(EventLog_RecordIsValid(record) != 0U)
        {
            sequence = EventLog_ReadUint32(&record[4]);
            s_stored_count++;

            if((sequence_found == 0U) ||
               ((int32_t)(sequence - highest_sequence) > 0))
            {
                highest_sequence = sequence;
                sequence_found = 1U;
            }
        }
    }

    if(sequence_found != 0U)
    {
        s_next_sequence = highest_sequence + 1U;
    }

    s_initialized = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef EventLog_Append(EventLogType type,
                                  uint8_t data0,
                                  uint8_t data1)
{
    uint8_t record[EVENT_LOG_RECORD_SIZE];
    uint8_t verify_record[EVENT_LOG_RECORD_SIZE];
    uint8_t i;
    HAL_StatusTypeDef status;

    if((s_initialized == 0U) ||
       ((uint8_t)type < (uint8_t)EVENT_LOG_TYPE_BOOT) ||
       ((uint8_t)type > (uint8_t)EVENT_LOG_TYPE_LIGHT_TIMEOUT))
    {
        return HAL_ERROR;
    }

    if(s_next_address >= EVENT_LOG_SECTOR_END_ADDRESS)
    {
        status = BspFlash_EraseSector4K(EVENT_LOG_SECTOR_ADDRESS);
        if(status != HAL_OK)
        {
            return status;
        }

        s_next_address = EVENT_LOG_SECTOR_ADDRESS;
        s_stored_count = 0U;
    }

    EventLog_BuildRecord(record, type, data0, data1);

    status = BspFlash_ProgramPage(s_next_address,
                                  record,
                                  EVENT_LOG_RECORD_SIZE);
    if(status != HAL_OK)
    {
        return status;
    }

    status = BspFlash_Read(s_next_address,
                           verify_record,
                           EVENT_LOG_RECORD_SIZE);
    if(status != HAL_OK)
    {
        return status;
    }

    for(i = 0U; i < EVENT_LOG_RECORD_SIZE; i++)
    {
        if(verify_record[i] != record[i])
        {
            return HAL_ERROR;
        }
    }

    s_next_address += EVENT_LOG_RECORD_SIZE;
    s_next_sequence++;
    s_stored_count++;
    return HAL_OK;
}

uint16_t EventLog_GetStoredCount(void)
{
    return s_stored_count;
}

HAL_StatusTypeDef EventLog_ReadNewest(uint8_t newest_offset,
                                      EventLogRecord *record)
{
    uint8_t raw_record[EVENT_LOG_RECORD_SIZE];
    uint32_t address;
    uint32_t target_sequence;

    if((s_initialized == 0U) || (record == 0) ||
       ((uint16_t)newest_offset >= s_stored_count))
    {
        return HAL_ERROR;
    }

    target_sequence = s_next_sequence - 1U - newest_offset;

    for(address = EVENT_LOG_SECTOR_ADDRESS;
        address < EVENT_LOG_SECTOR_END_ADDRESS;
        address += EVENT_LOG_RECORD_SIZE)
    {
        if(BspFlash_Read(address,
                         raw_record,
                         EVENT_LOG_RECORD_SIZE) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if((EventLog_RecordIsValid(raw_record) != 0U) &&
           (EventLog_ReadUint32(&raw_record[4]) == target_sequence))
        {
            record->type = (EventLogType)raw_record[3];
            record->sequence = target_sequence;
            record->timestamp_ms = EventLog_ReadUint32(&raw_record[8]);
            record->data0 = raw_record[12];
            record->data1 = raw_record[13];
            return HAL_OK;
        }
    }

    return HAL_ERROR;
}

HAL_StatusTypeDef EventLog_Clear(void)
{
    HAL_StatusTypeDef status;

    if(s_initialized == 0U)
    {
        return HAL_ERROR;
    }

    status = BspFlash_EraseSector4K(EVENT_LOG_SECTOR_ADDRESS);
    if(status != HAL_OK)
    {
        return status;
    }

    s_next_address = EVENT_LOG_SECTOR_ADDRESS;
    s_next_sequence = 0U;
    s_stored_count = 0U;
    return HAL_OK;
}

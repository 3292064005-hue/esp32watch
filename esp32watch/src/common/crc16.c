#include "common/crc16.h"

uint16_t crc16_mix_u16(uint16_t crc, uint16_t value)
{
    crc ^= value;
    crc = (uint16_t)((crc << 5) | (crc >> 11));
    crc ^= (uint16_t)(value >> 3);
    return crc;
}

uint16_t crc16_buf(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0x1D0FU;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8U; ++b) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

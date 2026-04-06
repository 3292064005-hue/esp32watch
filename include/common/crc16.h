#ifndef COMMON_CRC16_H
#define COMMON_CRC16_H

#include <stdint.h>

uint16_t crc16_mix_u16(uint16_t crc, uint16_t value);
uint16_t crc16_buf(const uint8_t *data, uint32_t len);

#endif

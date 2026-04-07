#ifndef ESP32_MPU_I2C_H
#define ESP32_MPU_I2C_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

bool esp32_mpu_i2c_available(void);
bool esp32_mpu_i2c_init(void);
bool esp32_mpu_i2c_recover_bus(void);
PlatformStatus esp32_mpu_i2c_write(uint8_t address_7bit, const uint8_t *data, uint16_t size, uint32_t timeout_ms);
PlatformStatus esp32_mpu_i2c_read_reg(uint8_t address_7bit,
                                         uint8_t reg,
                                         uint8_t *data,
                                         uint16_t size,
                                         uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif


#ifndef BSP_I2C_H
#define BSP_I2C_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void bsp_i2c_config_output_od(PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin);
void bsp_i2c_config_peripheral(PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin);
bool bsp_i2c_recover_bus(PlatformI2cBus *bus, PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin);
PlatformStatus bsp_i2c_master_transmit_recoverable(PlatformI2cBus *bus,
                                                      PlatformGpioPort *port,
                                                      uint16_t scl_pin,
                                                      uint16_t sda_pin,
                                                      uint16_t address,
                                                      uint8_t *data,
                                                      uint16_t size,
                                                      uint32_t timeout_ms);
PlatformStatus bsp_i2c_mem_read_recoverable(PlatformI2cBus *bus,
                                               PlatformGpioPort *port,
                                               uint16_t scl_pin,
                                               uint16_t sda_pin,
                                               uint16_t address,
                                               uint16_t mem_address,
                                               uint16_t memadd_size,
                                               uint8_t *data,
                                               uint16_t size,
                                               uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif

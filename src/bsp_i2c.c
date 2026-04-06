#include "bsp_i2c.h"

void bsp_i2c_config_output_od(PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin)
{
    PlatformGpioConfig gpio = {0};
    gpio.Pin = scl_pin | sda_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    platform_GPIO_Init(port, &gpio);
}

void bsp_i2c_config_peripheral(PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin)
{
    PlatformGpioConfig gpio = {0};
    gpio.Pin = scl_pin | sda_pin;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    platform_GPIO_Init(port, &gpio);
}

bool bsp_i2c_recover_bus(I2C_HandleTypeDef *hi2c, PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin)
{
    if (hi2c == NULL || port == NULL) {
        return false;
    }

    (void)platform_I2C_DeInit(hi2c);
    bsp_i2c_config_output_od(port, scl_pin, sda_pin);
    platform_GPIO_WritePin(port, scl_pin | sda_pin, GPIO_PIN_SET);
    platform_Delay(1U);

    for (uint8_t i = 0U; i < 9U; ++i) {
        platform_GPIO_WritePin(port, scl_pin, GPIO_PIN_RESET);
        platform_Delay(1U);
        platform_GPIO_WritePin(port, scl_pin, GPIO_PIN_SET);
        platform_Delay(1U);
        if (platform_GPIO_ReadPin(port, sda_pin) == GPIO_PIN_SET) {
            break;
        }
    }

    platform_GPIO_WritePin(port, sda_pin, GPIO_PIN_RESET);
    platform_Delay(1U);
    platform_GPIO_WritePin(port, scl_pin, GPIO_PIN_SET);
    platform_Delay(1U);
    platform_GPIO_WritePin(port, sda_pin, GPIO_PIN_SET);
    platform_Delay(1U);

    bsp_i2c_config_peripheral(port, scl_pin, sda_pin);
    platform_Delay(1U);
    return platform_I2C_Init(hi2c) == platform_OK;
}

platform_StatusTypeDef bsp_i2c_master_transmit_recoverable(I2C_HandleTypeDef *hi2c,
                                                      PlatformGpioPort *port,
                                                      uint16_t scl_pin,
                                                      uint16_t sda_pin,
                                                      uint16_t address,
                                                      uint8_t *data,
                                                      uint16_t size,
                                                      uint32_t timeout_ms)
{
    platform_StatusTypeDef st;
    st = platform_I2C_Master_Transmit(hi2c, address, data, size, timeout_ms);
    if (st == platform_OK) {
        return st;
    }
    if (!bsp_i2c_recover_bus(hi2c, port, scl_pin, sda_pin)) {
        return st;
    }
    return platform_I2C_Master_Transmit(hi2c, address, data, size, timeout_ms);
}

platform_StatusTypeDef bsp_i2c_mem_read_recoverable(I2C_HandleTypeDef *hi2c,
                                               PlatformGpioPort *port,
                                               uint16_t scl_pin,
                                               uint16_t sda_pin,
                                               uint16_t address,
                                               uint16_t mem_address,
                                               uint16_t memadd_size,
                                               uint8_t *data,
                                               uint16_t size,
                                               uint32_t timeout_ms)
{
    platform_StatusTypeDef st;
    st = platform_I2C_Mem_Read(hi2c, address, mem_address, memadd_size, data, size, timeout_ms);
    if (st == platform_OK) {
        return st;
    }
    if (!bsp_i2c_recover_bus(hi2c, port, scl_pin, sda_pin)) {
        return st;
    }
    return platform_I2C_Mem_Read(hi2c, address, mem_address, memadd_size, data, size, timeout_ms);
}




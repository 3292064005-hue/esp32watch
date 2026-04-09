#include "platform_i2c_recovery.h"
#include "app_config.h"

#if !defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
#include "bsp_i2c.h"
#endif

static PlatformGpioConfig make_od_config(uint16_t pin_mask)
{
    PlatformGpioConfig cfg = {};
    cfg.pin_mask = pin_mask;
    cfg.mode = PLATFORM_GPIO_MODE_OUTPUT_OD;
    cfg.pull = PLATFORM_GPIO_PULL_UP;
    cfg.speed = PLATFORM_GPIO_SPEED_HIGH;
    return cfg;
}

void platform_i2c_config_output_od(PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    PlatformGpioConfig cfg = make_od_config((uint16_t)(scl_pin | sda_pin));
    platform_gpio_init(port, &cfg);
    platform_gpio_write(port, scl_pin | sda_pin, PLATFORM_PIN_HIGH);
#else
    bsp_i2c_config_output_od(port, scl_pin, sda_pin);
#endif
}

void platform_i2c_config_peripheral(PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    (void)port;
    (void)scl_pin;
    (void)sda_pin;
    (void)platform_i2c_init(&platform_i2c_main);
#else
    bsp_i2c_config_peripheral(port, scl_pin, sda_pin);
#endif
}

PlatformStatus platform_i2c_recover_bus(PlatformI2cBus *bus, PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    (void)bus;
    platform_i2c_config_output_od(port, scl_pin, sda_pin);
    for (uint8_t i = 0; i < 9U; ++i) {
        platform_gpio_write(port, scl_pin, PLATFORM_PIN_HIGH);
        platform_delay_ms(1U);
        platform_gpio_write(port, scl_pin, PLATFORM_PIN_LOW);
        platform_delay_ms(1U);
    }
    platform_gpio_write(port, sda_pin, PLATFORM_PIN_LOW);
    platform_delay_ms(1U);
    platform_gpio_write(port, scl_pin, PLATFORM_PIN_HIGH);
    platform_delay_ms(1U);
    platform_gpio_write(port, sda_pin, PLATFORM_PIN_HIGH);
    platform_i2c_config_peripheral(port, scl_pin, sda_pin);
    return PLATFORM_STATUS_OK;
#else
    return bsp_i2c_recover_bus(bus, port, scl_pin, sda_pin);
#endif
}

PlatformStatus platform_i2c_master_transmit_recoverable(PlatformI2cBus *bus, PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin, uint16_t address, uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    if (platform_i2c_write(bus, address, data, size, timeout_ms) == PLATFORM_STATUS_OK) {
        return PLATFORM_STATUS_OK;
    }
    if (platform_i2c_recover_bus(bus, port, scl_pin, sda_pin) != PLATFORM_STATUS_OK) {
        return PLATFORM_STATUS_ERROR;
    }
    return platform_i2c_write(bus, address, data, size, timeout_ms);
#else
    return bsp_i2c_master_transmit_recoverable(bus, port, scl_pin, sda_pin, address, data, size, timeout_ms);
#endif
}

PlatformStatus platform_i2c_mem_read_recoverable(PlatformI2cBus *bus, PlatformGpioPort *port, uint16_t scl_pin, uint16_t sda_pin, uint16_t address, uint16_t mem_address, uint16_t mem_address_size, uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    if (platform_i2c_mem_read(bus, address, mem_address, mem_address_size, data, size, timeout_ms) == PLATFORM_STATUS_OK) {
        return PLATFORM_STATUS_OK;
    }
    if (platform_i2c_recover_bus(bus, port, scl_pin, sda_pin) != PLATFORM_STATUS_OK) {
        return PLATFORM_STATUS_ERROR;
    }
    return platform_i2c_mem_read(bus, address, mem_address, mem_address_size, data, size, timeout_ms);
#else
    return bsp_i2c_mem_read_recoverable(bus, port, scl_pin, sda_pin, address, mem_address, mem_address_size, data, size, timeout_ms);
#endif
}

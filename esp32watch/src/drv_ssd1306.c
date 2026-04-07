#include "drv_ssd1306.h"
#include "app_config.h"
#include "board_manifest.h"
#include "bsp_i2c.h"
#include <string.h>


static const BoardManifest *ssd1306_manifest(void)
{
    return board_manifest_get();
}

PlatformStatus drv_ssd1306_write_cmds(const uint8_t *cmds, uint8_t count)
{
    uint8_t packet[17];
    const BoardManifest *manifest = ssd1306_manifest();
    if (cmds == NULL || count == 0U || manifest == NULL || manifest->i2c_port == NULL) {
        return HAL_ERROR;
    }
    if (count > 16U) {
        count = 16U;
    }
    packet[0] = 0x00U;
    memcpy(&packet[1], cmds, count);
    return bsp_i2c_master_transmit_recoverable(&OLED_I2C_HANDLE,
                                               manifest->i2c_port,
                                               manifest->i2c_scl_pin,
                                               manifest->i2c_sda_pin,
                                               OLED_I2C_ADDRESS,
                                               packet,
                                               (uint16_t)(count + 1U),
                                               OLED_I2C_TIMEOUT_MS);
}

PlatformStatus drv_ssd1306_write_data(const uint8_t *data, uint8_t len)
{
    uint8_t packet[1 + OLED_I2C_CHUNK_BYTES];
    const BoardManifest *manifest = ssd1306_manifest();
    if (data == NULL || len == 0U || manifest == NULL || manifest->i2c_port == NULL) {
        return HAL_ERROR;
    }
    if (len > OLED_I2C_CHUNK_BYTES) {
        len = OLED_I2C_CHUNK_BYTES;
    }
    packet[0] = 0x40U;
    memcpy(&packet[1], data, len);
    return bsp_i2c_master_transmit_recoverable(&OLED_I2C_HANDLE,
                                               manifest->i2c_port,
                                               manifest->i2c_scl_pin,
                                               manifest->i2c_sda_pin,
                                               OLED_I2C_ADDRESS,
                                               packet,
                                               (uint16_t)(len + 1U),
                                               OLED_I2C_TIMEOUT_MS);
}

PlatformStatus drv_ssd1306_init_panel(void)
{
    const uint8_t init_cmds[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0x9F,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0x2E,
        0xAF
    };
    PlatformStatus st = HAL_OK;
    uint8_t idx = 0U;
    const BoardManifest *manifest = ssd1306_manifest();

#if OLED_RESET_ENABLED
    platform_gpio_write(OLED_RESET_GPIO_Port, OLED_RESET_Pin, GPIO_PIN_RESET);
    platform_delay_ms(10U);
    platform_gpio_write(OLED_RESET_GPIO_Port, OLED_RESET_Pin, GPIO_PIN_SET);
    platform_delay_ms(10U);
#else
    platform_delay_ms(40U);
#endif

    if (manifest != NULL && manifest->i2c_port != NULL) {
        (void)bsp_i2c_recover_bus(&OLED_I2C_HANDLE,
                                  manifest->i2c_port,
                                  manifest->i2c_scl_pin,
                                  manifest->i2c_sda_pin);
        platform_delay_ms(5U);
    }

    while (idx < sizeof(init_cmds)) {
        uint8_t batch = 1U;
        uint8_t cmd = init_cmds[idx];
        if ((idx + 1U) < sizeof(init_cmds) &&
            (cmd == 0xD5U || cmd == 0xA8U || cmd == 0xD3U || cmd == 0x8DU || cmd == 0x20U ||
             cmd == 0xDAU || cmd == 0x81U || cmd == 0xD9U || cmd == 0xDBU)) {
            batch = 2U;
        }
        st = drv_ssd1306_write_cmds(&init_cmds[idx], batch);
        if (st != HAL_OK) {
            return st;
        }
        idx = (uint8_t)(idx + batch);
    }
    return HAL_OK;
}

PlatformStatus drv_ssd1306_set_contrast(uint8_t value)
{
    uint8_t cmds[] = {0x81U, value};
    return drv_ssd1306_write_cmds(cmds, (uint8_t)sizeof(cmds));
}

PlatformStatus drv_ssd1306_set_sleep(bool sleep)
{
    uint8_t cmd = sleep ? 0xAEU : 0xAFU;
    return drv_ssd1306_write_cmds(&cmd, 1U);
}




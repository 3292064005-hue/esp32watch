#ifndef BOARD_MANIFEST_H
#define BOARD_MANIFEST_H

#include <stdint.h>
#include "board_features.h"
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *profile_name;
    bool battery_enabled;
    bool vibration_enabled;
    bool buzzer_enabled;
    bool sensor_enabled;
    bool watchdog_enabled;
    bool flash_storage_enabled;
    bool mirror_bkp_enabled;
    bool reset_domain_storage_enabled;
    bool idle_light_sleep_enabled;
    uint32_t flash_page_size;
    uint32_t flash_page0_address;
    uint32_t flash_page1_address;
    PlatformGpioPort *i2c_port;
    uint16_t i2c_scl_pin;
    uint16_t i2c_sda_pin;
    PlatformGpioPort *vibe_port;
    uint16_t vibe_pin;
    int16_t buzzer_gpio;
    int16_t i2c_scl_gpio;
    int16_t i2c_sda_gpio;
    uint32_t i2c_clock_hz;
    int16_t mpu_i2c_scl_gpio;
    int16_t mpu_i2c_sda_gpio;
    uint32_t mpu_i2c_clock_hz;
    int16_t key_up_gpio;
    int16_t key_down_gpio;
    int16_t key_ok_gpio;
    int16_t key_back_gpio;
    int16_t battery_adc_gpio;
    int16_t charge_det_gpio;
    int16_t vibe_gpio;
    int16_t oled_reset_gpio;
    uint8_t key_count;
    bool all_keys_mapped;
#if APP_FEATURE_COMPANION_UART
    int16_t companion_uart_tx_gpio;
    int16_t companion_uart_rx_gpio;
#endif
} BoardManifest;

const BoardManifest *board_manifest_get(void);
const char *board_manifest_profile_name(void);
bool board_manifest_storage_map_valid(void);
uint32_t board_manifest_storage_reserved_bytes(void);
int16_t board_manifest_resolve_native_gpio(PlatformGpioPort *port, uint16_t pin_mask);
bool board_manifest_keypad_mapping_valid(void);
bool board_manifest_gpio_contract_valid(void);

#ifdef __cplusplus
}
#endif

#endif

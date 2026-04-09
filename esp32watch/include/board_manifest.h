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
    uint32_t flash_page_size;
    uint32_t flash_page0_address;
    uint32_t flash_page1_address;
    PlatformGpioPort *i2c_port;
    uint16_t i2c_scl_pin;
    uint16_t i2c_sda_pin;
    PlatformGpioPort *vibe_port;
    uint16_t vibe_pin;
    int16_t buzzer_gpio;
} BoardManifest;

const BoardManifest *board_manifest_get(void);
const char *board_manifest_profile_name(void);
bool board_manifest_storage_map_valid(void);
uint32_t board_manifest_storage_reserved_bytes(void);

#ifdef __cplusplus
}
#endif

#endif

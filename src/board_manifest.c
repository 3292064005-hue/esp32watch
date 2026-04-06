#include "board_manifest.h"
#include "board_storage_map.h"

static const BoardManifest g_manifest = {
#if defined(APP_BOARD_PROFILE_BLUEPILL_FULL)
    .profile_name = "bluepill_full",
#elif defined(APP_BOARD_PROFILE_BLUEPILL_BATTERY)
    .profile_name = "bluepill_battery",
#elif defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    .profile_name = "esp32s3_watch",
#else
    .profile_name = "bluepill_core",
#endif
    .battery_enabled = APP_FEATURE_BATTERY != 0,
    .vibration_enabled = APP_FEATURE_VIBRATION != 0,
    .sensor_enabled = APP_FEATURE_SENSOR != 0,
    .watchdog_enabled = APP_FEATURE_WATCHDOG != 0,
    .flash_storage_enabled = APP_STORAGE_USE_FLASH != 0,
    .mirror_bkp_enabled = APP_STORAGE_MIRROR_BKP_WHEN_FLASH != 0,
    .flash_page_size = FLASH_STORAGE_PAGE_SIZE,
    .flash_page0_address = FLASH_STORAGE_PAGE0_ADDRESS,
    .flash_page1_address = FLASH_STORAGE_PAGE1_ADDRESS,
    .i2c_port = PLATFORM_GPIOB,
    .i2c_scl_pin = PLATFORM_PIN_6,
    .i2c_sda_pin = PLATFORM_PIN_7,
    .vibe_port = PLATFORM_GPIOB,
    .vibe_pin = PLATFORM_PIN_0,
};

const BoardManifest *board_manifest_get(void)
{
    return &g_manifest;
}

const char *board_manifest_profile_name(void)
{
    return g_manifest.profile_name;
}

bool board_manifest_storage_map_valid(void)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    return true;
#else
    return g_manifest.flash_page_size == FLASH_STORAGE_PAGE_SIZE &&
           g_manifest.flash_page0_address == FLASH_STORAGE_PAGE0_ADDRESS &&
           g_manifest.flash_page1_address == FLASH_STORAGE_PAGE1_ADDRESS &&
           g_manifest.flash_page_size != 0U &&
           g_manifest.flash_page1_address == (g_manifest.flash_page0_address + g_manifest.flash_page_size) &&
           g_manifest.flash_page0_address >= 0x08000000UL &&
           board_manifest_storage_reserved_bytes() == FLASH_STORAGE_RESERVED_BYTES &&
           FLASH_STORAGE_REGION_END == FLASH_STORAGE_EXPECTED_FLASH_END;
#endif
}

uint32_t board_manifest_storage_reserved_bytes(void)
{
    return g_manifest.flash_page_size * 2U;
}

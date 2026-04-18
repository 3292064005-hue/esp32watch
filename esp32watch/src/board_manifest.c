#include "board_manifest.h"
#include "board_storage_map.h"
#include "esp32_port_config.h"
#include "esp32_partition_contract.h"
#include "app_config.h"
#include <stddef.h>
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH) && defined(ARDUINO_ARCH_ESP32)
#include <esp_partition.h>
#endif


#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH) && defined(ARDUINO_ARCH_ESP32)
static bool esp32_partition_matches(esp_partition_type_t type,
                                    esp_partition_subtype_t subtype,
                                    const char *label,
                                    uint32_t expected_offset,
                                    uint32_t expected_size)
{
    const esp_partition_t *partition = esp_partition_find_first(type, subtype, label);
    return partition != NULL &&
           partition->address == expected_offset &&
           partition->size == expected_size;
}
#endif

static bool gpio_is_mapped(int16_t gpio)
{
    return gpio >= 0;
}

static bool gpio_value_conflicts(int16_t gpio, int16_t other)
{
    return gpio_is_mapped(gpio) && gpio_is_mapped(other) && gpio == other;
}

static bool board_manifest_key_gpio_unique(const BoardManifest *manifest)
{
    if (manifest == NULL) {
        return false;
    }

    return !gpio_value_conflicts(manifest->key_up_gpio, manifest->key_down_gpio) &&
           !gpio_value_conflicts(manifest->key_up_gpio, manifest->key_ok_gpio) &&
           !gpio_value_conflicts(manifest->key_up_gpio, manifest->key_back_gpio) &&
           !gpio_value_conflicts(manifest->key_down_gpio, manifest->key_ok_gpio) &&
           !gpio_value_conflicts(manifest->key_down_gpio, manifest->key_back_gpio) &&
           !gpio_value_conflicts(manifest->key_ok_gpio, manifest->key_back_gpio);
}

static bool board_manifest_key_gpio_conflicts_with_peripherals(const BoardManifest *manifest)
{
    const int16_t peripheral_gpios[] = {
        manifest != NULL ? manifest->i2c_scl_gpio : -1,
        manifest != NULL ? manifest->i2c_sda_gpio : -1,
        manifest != NULL ? manifest->mpu_i2c_scl_gpio : -1,
        manifest != NULL ? manifest->mpu_i2c_sda_gpio : -1,
        manifest != NULL ? manifest->battery_adc_gpio : -1,
        manifest != NULL ? manifest->charge_det_gpio : -1,
        manifest != NULL ? manifest->vibe_gpio : -1,
        manifest != NULL ? manifest->oled_reset_gpio : -1,
        manifest != NULL ? manifest->buzzer_gpio : -1,
    #if APP_FEATURE_COMPANION_UART
        manifest != NULL ? manifest->companion_uart_tx_gpio : -1,
        manifest != NULL ? manifest->companion_uart_rx_gpio : -1,
    #endif
    };
    const int16_t key_gpios[] = {
        manifest != NULL ? manifest->key_up_gpio : -1,
        manifest != NULL ? manifest->key_down_gpio : -1,
        manifest != NULL ? manifest->key_ok_gpio : -1,
        manifest != NULL ? manifest->key_back_gpio : -1,
    };

    if (manifest == NULL) {
        return true;
    }

    for (size_t key_index = 0U; key_index < (sizeof(key_gpios) / sizeof(key_gpios[0])); ++key_index) {
        if (!gpio_is_mapped(key_gpios[key_index])) {
            continue;
        }
        for (size_t periph_index = 0U; periph_index < (sizeof(peripheral_gpios) / sizeof(peripheral_gpios[0])); ++periph_index) {
            if (gpio_value_conflicts(key_gpios[key_index], peripheral_gpios[periph_index])) {
                return true;
            }
        }
    }

    return false;
}

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
    .buzzer_enabled = APP_FEATURE_BUZZER != 0,
    .sensor_enabled = APP_FEATURE_SENSOR != 0,
    .watchdog_enabled = APP_FEATURE_WATCHDOG != 0,
    .flash_storage_enabled = APP_STORAGE_USE_FLASH != 0,
    .mirror_bkp_enabled = APP_STORAGE_MIRROR_BKP_WHEN_FLASH != 0,
    .reset_domain_storage_enabled = true,
    .idle_light_sleep_enabled = ESP32_IDLE_LIGHT_SLEEP_ENABLED != 0,
    .flash_page_size = FLASH_STORAGE_PAGE_SIZE,
    .flash_page0_address = FLASH_STORAGE_PAGE0_ADDRESS,
    .flash_page1_address = FLASH_STORAGE_PAGE1_ADDRESS,
    .i2c_port = PLATFORM_GPIOB,
    .i2c_scl_pin = PLATFORM_PIN_6,
    .i2c_sda_pin = PLATFORM_PIN_7,
    .vibe_port = PLATFORM_GPIOB,
    .vibe_pin = PLATFORM_PIN_0,
    .buzzer_gpio = ESP32_BUZZER_GPIO,
    .i2c_scl_gpio = ESP32_I2C_SCL_GPIO,
    .i2c_sda_gpio = ESP32_I2C_SDA_GPIO,
    .i2c_clock_hz = ESP32_I2C_CLOCK_HZ,
    .mpu_i2c_scl_gpio = ESP32_MPU_I2C_SCL_GPIO,
    .mpu_i2c_sda_gpio = ESP32_MPU_I2C_SDA_GPIO,
    .mpu_i2c_clock_hz = ESP32_MPU_I2C_CLOCK_HZ,
    .key_up_gpio = ESP32_KEY_UP_GPIO,
    .key_down_gpio = ESP32_KEY_DOWN_GPIO,
    .key_ok_gpio = ESP32_KEY_OK_GPIO,
    .key_back_gpio = ESP32_KEY_BACK_GPIO,
    .battery_adc_gpio = ESP32_BATTERY_ADC_GPIO,
    .charge_det_gpio = ESP32_CHARGE_DET_GPIO,
    .vibe_gpio = ESP32_VIBE_GPIO,
    .oled_reset_gpio = ESP32_OLED_RESET_GPIO,
    .key_count = 4U,
    .all_keys_mapped = ESP32_KEY_UP_GPIO >= 0 &&
                       ESP32_KEY_DOWN_GPIO >= 0 &&
                       ESP32_KEY_OK_GPIO >= 0 &&
                       ESP32_KEY_BACK_GPIO >= 0,
#if APP_FEATURE_COMPANION_UART
    .companion_uart_tx_gpio = ESP32_COMPANION_TX_GPIO,
    .companion_uart_rx_gpio = ESP32_COMPANION_RX_GPIO,
#endif
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
    const bool layout_valid = ESP32_PARTITION_NVS_OFFSET < ESP32_PARTITION_OTADATA_OFFSET &&
                              ESP32_PARTITION_OTADATA_OFFSET < ESP32_PARTITION_APP0_OFFSET &&
                              ESP32_PARTITION_APP0_OFFSET < ESP32_PARTITION_APP1_OFFSET &&
                              ESP32_PARTITION_APP1_OFFSET < ESP32_PARTITION_LITTLEFS_OFFSET &&
                              ESP32_PARTITION_LITTLEFS_OFFSET < ESP32_PARTITION_COREDUMP_OFFSET &&
                              (ESP32_PARTITION_NVS_OFFSET + ESP32_PARTITION_NVS_SIZE) <= ESP32_PARTITION_OTADATA_OFFSET &&
                              (ESP32_PARTITION_OTADATA_OFFSET + ESP32_PARTITION_OTADATA_SIZE) <= ESP32_PARTITION_APP0_OFFSET &&
                              (ESP32_PARTITION_APP0_OFFSET + ESP32_PARTITION_APP0_SIZE) <= ESP32_PARTITION_APP1_OFFSET &&
                              (ESP32_PARTITION_APP1_OFFSET + ESP32_PARTITION_APP1_SIZE) <= ESP32_PARTITION_LITTLEFS_OFFSET &&
                              (ESP32_PARTITION_LITTLEFS_OFFSET + ESP32_PARTITION_LITTLEFS_SIZE) <= ESP32_PARTITION_COREDUMP_OFFSET &&
                              (ESP32_PARTITION_COREDUMP_OFFSET + ESP32_PARTITION_COREDUMP_SIZE) <= ESP32_FLASH_SIZE_BYTES &&
                              ESP32_PARTITION_LITTLEFS_SIZE != 0UL &&
                              ESP32_FLASH_SIZE_BYTES == (16UL * 1024UL * 1024UL);
#if defined(ARDUINO_ARCH_ESP32)
    if (!layout_valid) {
        return false;
    }
    return esp32_partition_matches(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs", ESP32_PARTITION_NVS_OFFSET, ESP32_PARTITION_NVS_SIZE) &&
           esp32_partition_matches(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, "otadata", ESP32_PARTITION_OTADATA_OFFSET, ESP32_PARTITION_OTADATA_SIZE) &&
           esp32_partition_matches(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0", ESP32_PARTITION_APP0_OFFSET, ESP32_PARTITION_APP0_SIZE) &&
           esp32_partition_matches(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "app1", ESP32_PARTITION_APP1_OFFSET, ESP32_PARTITION_APP1_SIZE) &&
           esp32_partition_matches(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, ESP32_PARTITION_LITTLEFS_LABEL, ESP32_PARTITION_LITTLEFS_OFFSET, ESP32_PARTITION_LITTLEFS_SIZE) &&
           esp32_partition_matches(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump", ESP32_PARTITION_COREDUMP_OFFSET, ESP32_PARTITION_COREDUMP_SIZE);
#else
    return layout_valid;
#endif
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


bool board_manifest_keypad_mapping_valid(void)
{
    return g_manifest.key_count >= 4U &&
           g_manifest.all_keys_mapped &&
           board_manifest_key_gpio_unique(&g_manifest);
}

bool board_manifest_gpio_contract_valid(void)
{
    return board_manifest_keypad_mapping_valid() &&
           !board_manifest_key_gpio_conflicts_with_peripherals(&g_manifest);
}

int16_t board_manifest_resolve_native_gpio(PlatformGpioPort *port, uint16_t pin_mask)
{
    if (port == NULL || pin_mask == 0U) {
        return -1;
    }
    if (port == g_manifest.i2c_port && pin_mask == g_manifest.i2c_scl_pin) return g_manifest.i2c_scl_gpio;
    if (port == g_manifest.i2c_port && pin_mask == g_manifest.i2c_sda_pin) return g_manifest.i2c_sda_gpio;
    if (port == KEY_UP_GPIO_Port && pin_mask == KEY_UP_Pin) return g_manifest.key_up_gpio;
    if (port == KEY_DOWN_GPIO_Port && pin_mask == KEY_DOWN_Pin) return g_manifest.key_down_gpio;
    if (port == KEY_OK_GPIO_Port && pin_mask == KEY_OK_Pin) return g_manifest.key_ok_gpio;
    if (port == KEY_BACK_GPIO_Port && pin_mask == KEY_BACK_Pin) return g_manifest.key_back_gpio;
    if (port == BATTERY_ADC_GPIO_Port && pin_mask == BATTERY_ADC_Pin) return g_manifest.battery_adc_gpio;
    if (port == CHARGE_DET_GPIO_Port && pin_mask == CHARGE_DET_Pin) return g_manifest.charge_det_gpio;
    if (port == g_manifest.vibe_port && pin_mask == g_manifest.vibe_pin) return g_manifest.vibe_gpio;
    if (port == OLED_RESET_GPIO_Port && pin_mask == OLED_RESET_Pin) return g_manifest.oled_reset_gpio;
#if APP_FEATURE_COMPANION_UART
    if (port == COMPANION_UART_GPIO_Port && pin_mask == COMPANION_UART_TX_Pin) return g_manifest.companion_uart_tx_gpio;
    if (port == COMPANION_UART_GPIO_Port && pin_mask == COMPANION_UART_RX_Pin) return g_manifest.companion_uart_rx_gpio;
#endif
    return -1;
}

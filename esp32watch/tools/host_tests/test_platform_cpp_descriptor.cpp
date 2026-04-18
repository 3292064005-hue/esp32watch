#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
#include "Wire.h"
#include "Adafruit_SSD1306.h"
#include "esp_sleep.h"

extern "C" {
#include "board_manifest.h"
#include "display_internal.h"
#include "esp32_mpu_i2c.h"
#include "platform_api.h"
}

int main()
{
    const BoardManifest *manifest = board_manifest_get();
    assert(manifest != nullptr);
    assert(manifest->key_count == 4U);
    assert(manifest->all_keys_mapped);
    assert(board_manifest_keypad_mapping_valid());
    assert(board_manifest_gpio_contract_valid());
    assert(manifest->reset_domain_storage_enabled);
    assert(manifest->idle_light_sleep_enabled);

    PlatformSupportSnapshot support = {};
    assert(platform_get_support_snapshot(&support));
    assert(support.rtc_reset_domain_supported);
    assert(support.idle_light_sleep_supported);
    assert(!support.watchdog_supported);
    assert(!support.flash_journal_supported);

    host_stub_reset_arduino_io();
    host_stub_reset_wire();
    host_stub_reset_display();

    assert(platform_i2c_init(&platform_i2c_main) == PLATFORM_STATUS_OK);
    assert(host_stub_wire_last_begin_sda[0] == manifest->i2c_sda_gpio);
    assert(host_stub_wire_last_begin_scl[0] == manifest->i2c_scl_gpio);
    assert(host_stub_wire_last_clock_hz[0] == manifest->i2c_clock_hz);

    host_stub_analog_read_value = 1234;
    assert(platform_adc_start(&platform_adc_main) == PLATFORM_STATUS_OK);
    if (manifest->battery_adc_gpio >= 0) {
        assert(host_stub_last_analog_read_pin == manifest->battery_adc_gpio);
        assert(platform_adc_read(&platform_adc_main) == 1234U);
    }

    assert(esp32_mpu_i2c_available());
    assert(esp32_mpu_i2c_init());
    assert(host_stub_wire_last_begin_sda[1] == manifest->mpu_i2c_sda_gpio);
    assert(host_stub_wire_last_begin_scl[1] == manifest->mpu_i2c_scl_gpio);
    assert(host_stub_wire_last_clock_hz[1] == manifest->mpu_i2c_clock_hz);

    host_stub_reset_arduino_io();
    assert(esp32_mpu_i2c_recover_bus());
    assert(host_stub_last_pin_mode_pin == manifest->mpu_i2c_sda_gpio || host_stub_last_pin_mode_pin == manifest->mpu_i2c_scl_gpio);

    assert(display_backend_init());
    assert(host_stub_wire_last_begin_sda[0] == manifest->i2c_sda_gpio);
    assert(host_stub_wire_last_begin_scl[0] == manifest->i2c_scl_gpio);
    assert(host_stub_display_begin_called);
    assert(host_stub_display_display_called);

    host_stub_reset_sleep();
    assert(platform_light_sleep_for(6U));
    assert(host_stub_light_sleep_called);
    assert(host_stub_last_sleep_timer_us == 6000ULL);

    puts("[OK] platform/descriptor C++ behavior check passed");
    return 0;
}

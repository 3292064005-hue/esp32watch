#include "app_config.h"
#include "system_init.h"
#include "board_manifest.h"
#include "platform_api.h"
#include "reset_reason.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern "C" {
PlatformI2cBus platform_i2c_main = {0};
PlatformRtcDevice platform_rtc_main = {0};
PlatformAdcDevice platform_adc_main = {0};
PlatformUartPort platform_uart_main = {0};
PlatformWatchdog platform_watchdog_main = {0};

static int g_storage_init_calls = 0;
static int g_time_init_calls = 0;
static int g_network_init_calls = 0;
static bool g_storage_should_fail = false;

void platform_init(void) {}
PlatformStatus platform_i2c_init(PlatformI2cBus *) { return PLATFORM_STATUS_OK; }
PlatformStatus platform_rtc_init(PlatformRtcDevice *) { return PLATFORM_STATUS_OK; }
PlatformStatus platform_adc_init(PlatformAdcDevice *) { return PLATFORM_STATUS_OK; }
PlatformStatus platform_watchdog_init(PlatformWatchdog *) { return PLATFORM_STATUS_OK; }
void platform_rtc_backup_unlock(void) {}
void reset_reason_init(void) {}
ResetReason reset_reason_get(void) { return RESET_REASON_POWER_ON; }
void crash_capsule_init(ResetReason) {}
void crash_capsule_note_init_failure(uint8_t, uint8_t) {}
void runtime_event_service_reset(void) {}
void Error_Handler(void) {}

bool storage_service_init(void) { ++g_storage_init_calls; return !g_storage_should_fail; }
bool time_service_init(void) { ++g_time_init_calls; return system_runtime_service_stage_prerequisites_met(SYSTEM_INIT_STAGE_TIME_SERVICE); }
bool network_sync_service_init(void) { ++g_network_init_calls; return system_runtime_service_stage_prerequisites_met(SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE); }
bool web_server_init(void) { return true; }
bool web_server_console_ready(void) { return true; }

bool board_manifest_keypad_mapping_valid(void) { return true; }
bool board_manifest_gpio_contract_valid(void) { return true; }
bool board_manifest_storage_map_valid(void) { return true; }
const BoardManifest *board_manifest_get(void)
{
    static BoardManifest manifest = {
        .profile_name = "host",
        .battery_enabled = false,
        .vibration_enabled = false,
        .buzzer_enabled = false,
        .sensor_enabled = false,
        .watchdog_enabled = false,
        .flash_storage_enabled = true,
        .mirror_bkp_enabled = true,
        .reset_domain_storage_enabled = true,
        .idle_light_sleep_enabled = true,
        .flash_page_size = 4096U,
        .flash_page0_address = 0x10000U,
        .flash_page1_address = 0x11000U,
        .i2c_port = reinterpret_cast<PlatformGpioPort *>(1),
        .i2c_scl_pin = 1,
        .i2c_sda_pin = 1,
        .vibe_port = reinterpret_cast<PlatformGpioPort *>(1),
        .vibe_pin = 1,
        .buzzer_gpio = -1,
        .i2c_scl_gpio = 1,
        .i2c_sda_gpio = 1,
        .i2c_clock_hz = 400000U,
        .mpu_i2c_scl_gpio = 1,
        .mpu_i2c_sda_gpio = 1,
        .mpu_i2c_clock_hz = 400000U,
        .key_up_gpio = 1,
        .key_down_gpio = 1,
        .key_ok_gpio = 1,
        .key_back_gpio = 1,
        .battery_adc_gpio = -1,
        .charge_det_gpio = -1,
        .vibe_gpio = -1,
        .oled_reset_gpio = -1,
        .key_count = 4,
        .all_keys_mapped = true,
    };
    return &manifest;
}
}

static void reset_counters(void)
{
    g_storage_init_calls = 0;
    g_time_init_calls = 0;
    g_network_init_calls = 0;
    g_storage_should_fail = false;
}

static void assert_expected_plan(void)
{
#if SYSTEM_RUNTIME_INIT_STORAGE_AUTHORITY_FIRST
    assert(system_runtime_init_profile() == SYSTEM_RUNTIME_INIT_PROFILE_STORAGE_AUTHORITY_FIRST);
    assert(strcmp(system_runtime_init_profile_name(), "STORAGE_AUTHORITY_FIRST") == 0);
    assert(system_runtime_service_plan_length() == 3U);
    assert(system_runtime_service_plan_stage(0U) == SYSTEM_INIT_STAGE_STORAGE_SERVICE);
    assert(system_runtime_service_plan_stage(1U) == SYSTEM_INIT_STAGE_TIME_SERVICE);
    assert(system_runtime_service_plan_stage(2U) == SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE);
    assert(!system_runtime_service_stage_prerequisites_met(SYSTEM_INIT_STAGE_TIME_SERVICE));
    assert(!system_runtime_service_stage_prerequisites_met(SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE));
#else
    assert(system_runtime_init_profile() == SYSTEM_RUNTIME_INIT_PROFILE_LEGACY_CONSUMER_FIRST);
    assert(strcmp(system_runtime_init_profile_name(), "LEGACY_CONSUMER_FIRST") == 0);
    assert(system_runtime_service_plan_length() == 3U);
    assert(system_runtime_service_plan_stage(0U) == SYSTEM_INIT_STAGE_TIME_SERVICE);
    assert(system_runtime_service_plan_stage(1U) == SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE);
    assert(system_runtime_service_plan_stage(2U) == SYSTEM_INIT_STAGE_STORAGE_SERVICE);
    assert(system_runtime_service_stage_prerequisites_met(SYSTEM_INIT_STAGE_TIME_SERVICE));
    assert(!system_runtime_service_stage_prerequisites_met(SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE));
#endif
}

static void assert_success_path(void)
{
    assert(system_runtime_service_init());
    assert(g_storage_init_calls == 1);
    assert(g_time_init_calls == 1);
    assert(g_network_init_calls == 1);
    assert(system_init_stage_completed(SYSTEM_INIT_STAGE_STORAGE_SERVICE));
    assert(system_init_stage_completed(SYSTEM_INIT_STAGE_TIME_SERVICE));
    assert(system_init_stage_completed(SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE));
}

static void assert_failure_path(void)
{
#if SYSTEM_RUNTIME_INIT_STORAGE_AUTHORITY_FIRST
    g_storage_should_fail = true;
    assert(!system_runtime_service_init());
    assert(g_storage_init_calls == 1);
    assert(g_time_init_calls == 0);
    assert(g_network_init_calls == 0);
#else
    g_storage_should_fail = true;
    assert(!system_runtime_service_init());
    assert(g_storage_init_calls == 1);
    assert(g_time_init_calls == 1);
    assert(g_network_init_calls == 1);
#endif
    assert(system_init_has_failed());
    assert(system_last_fatal_code() == SYSTEM_FATAL_CODE_INIT_STAGE_FAILURE);
    assert(system_last_fatal_policy() == SYSTEM_FATAL_POLICY_STOP);
    assert(system_last_fatal_detail() == 0U);
}

int main(void)
{
    reset_counters();
    system_bootstrap();
    assert_expected_plan();
    assert_success_path();

    reset_counters();
    system_bootstrap();
    assert_failure_path();

#if SYSTEM_RUNTIME_INIT_STORAGE_AUTHORITY_FIRST
    puts("[OK] system_runtime_service_init STORAGE_AUTHORITY_FIRST order/assertion check passed");
#else
    puts("[OK] system_runtime_service_init LEGACY_CONSUMER_FIRST order/assertion check passed");
#endif
    return 0;
}

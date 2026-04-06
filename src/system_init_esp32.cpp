#include <Arduino.h>
#include <Wire.h>
extern "C" {
#include "system_init.h"
#include "main.h"
#include "board_features.h"
#include "app_config.h"
#include "reset_reason.h"
#include "board_manifest.h"
#include "crash_capsule.h"
#include "services/storage_service.h"
#include "services/time_service.h"
#include "services/network_sync_service.h"
#include "system_init_stage_internal.h"
#include "platform_api.h"
}
#include <string.h>

static SystemInitStage g_last_init_stage;
static bool g_init_failed;
static SystemRuntimeCapabilities g_runtime_caps;
static SystemRuntimeCapabilities g_runtime_caps_disabled;
static SystemInitCompletionState g_init_completed;
static SystemFatalCode g_last_fatal_code;
static SystemFatalPolicy g_last_fatal_policy;
static uint32_t g_last_fatal_detail;
static bool g_safe_mode_boot_recovery_pending;
static SystemInitStage g_safe_mode_boot_recovery_stage;

extern "C" PlatformI2cBus platform_i2c_main;
extern "C" PlatformRtcDevice platform_rtc_main;
extern "C" PlatformAdcDevice platform_adc_main;
extern "C" PlatformUartPort platform_uart_main;
extern "C" PlatformWatchdog platform_watchdog_main;

static void system_record_stage(SystemInitStage stage) { g_last_init_stage = stage; }

static void system_apply_same_boot_recovery(SystemInitStage stage)
{
    switch (stage) {
        case SYSTEM_INIT_STAGE_ADC:
            g_runtime_caps_disabled.has_battery_adc = true;
            break;
        case SYSTEM_INIT_STAGE_IWDG:
            g_runtime_caps_disabled.has_watchdog = true;
            break;
        default:
            break;
    }
    g_safe_mode_boot_recovery_pending = true;
    g_safe_mode_boot_recovery_stage = stage;
}

extern "C" void system_raise_fatal(SystemFatalCode code, SystemInitStage stage, uint32_t detail, SystemFatalPolicy policy)
{
    g_last_fatal_code = code;
    g_last_fatal_policy = policy;
    g_last_fatal_detail = detail;
    system_record_stage(stage);
    crash_capsule_note_init_failure((uint8_t)stage, (uint8_t)policy);

    if (policy == SYSTEM_FATAL_POLICY_SAFE_MODE && system_init_stage_supports_same_boot_safe_mode(stage)) {
        system_apply_same_boot_recovery(stage);
        return;
    }

    g_init_failed = true;
    Error_Handler();
}

static bool MX_GPIO_Init(void)
{
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
#endif
    return true;
}

static bool MX_I2C1_Init(void)
{
    return platform_i2c_init(&platform_i2c_main) == PLATFORM_STATUS_OK;
}

static bool MX_RTC_Init(void)
{
    return platform_rtc_init(&platform_rtc_main) == PLATFORM_STATUS_OK;
}

static bool MX_BKP_Init(void)
{
    platform_rtc_backup_unlock();
    return true;
}

static bool MX_ADC1_Init(void)
{
#if APP_FEATURE_BATTERY
    return platform_adc_init(&platform_adc_main) == PLATFORM_STATUS_OK;
#else
    return true;
#endif
}

static bool MX_IWDG_Init(void)
{
#if APP_FEATURE_WATCHDOG
    return platform_watchdog_init(&platform_watchdog_main) == PLATFORM_STATUS_OK;
#else
    return true;
#endif
}

extern "C" void system_bootstrap(void)
{
    g_init_failed = false;
    g_last_fatal_code = SYSTEM_FATAL_CODE_NONE;
    g_last_fatal_policy = SYSTEM_FATAL_POLICY_STOP;
    g_last_fatal_detail = 0U;
    g_safe_mode_boot_recovery_pending = false;
    g_safe_mode_boot_recovery_stage = SYSTEM_INIT_STAGE_NONE;
    memset(&g_runtime_caps, 0, sizeof(g_runtime_caps));
    memset(&g_runtime_caps_disabled, 0, sizeof(g_runtime_caps_disabled));
    system_init_completion_reset(&g_init_completed);
    platform_init();
    system_record_stage(SYSTEM_INIT_STAGE_HAL);
    reset_reason_init();
    crash_capsule_init(reset_reason_get());
    system_record_stage(SYSTEM_INIT_STAGE_RESET_REASON);
    SystemClock_Config();
    system_record_stage(SYSTEM_INIT_STAGE_CLOCK);
}

extern "C" bool system_runtime_capability_probe(SystemRuntimeCapabilities *out)
{
    const BoardManifest *manifest = board_manifest_get();

    system_record_stage(SYSTEM_INIT_STAGE_CAPABILITY_PROBE);
    memset(&g_runtime_caps, 0, sizeof(g_runtime_caps));
    if (manifest == NULL) {
        if (out != NULL) {
            *out = g_runtime_caps;
        }
        return false;
    }

    g_runtime_caps.has_battery_adc = manifest->battery_enabled &&
                                     !g_runtime_caps_disabled.has_battery_adc &&
                                     system_init_stage_completed(SYSTEM_INIT_STAGE_ADC);
    g_runtime_caps.has_vibration = manifest->vibration_enabled;
    g_runtime_caps.has_sensor = manifest->sensor_enabled;
    g_runtime_caps.has_watchdog = manifest->watchdog_enabled &&
                                  !g_runtime_caps_disabled.has_watchdog &&
                                  system_init_stage_completed(SYSTEM_INIT_STAGE_IWDG);
    g_runtime_caps.has_flash_storage = manifest->flash_storage_enabled;
    g_runtime_caps.has_bkp_mirror = manifest->mirror_bkp_enabled;
    g_runtime_caps.storage_map_valid = !manifest->flash_storage_enabled || board_manifest_storage_map_valid();

    if (out != NULL) {
        *out = g_runtime_caps;
    }

    return manifest->profile_name != NULL &&
           (!manifest->sensor_enabled || (manifest->i2c_port != NULL && manifest->i2c_scl_pin != 0U && manifest->i2c_sda_pin != 0U)) &&
           (!manifest->vibration_enabled || (manifest->vibe_port != NULL && manifest->vibe_pin != 0U)) &&
           g_runtime_caps.storage_map_valid;
}

extern "C" bool system_init_has_failed(void) { return g_init_failed; }
extern "C" SystemFatalCode system_last_fatal_code(void) { return g_last_fatal_code; }
extern "C" SystemFatalPolicy system_last_fatal_policy(void) { return g_last_fatal_policy; }
extern "C" uint32_t system_last_fatal_detail(void) { return g_last_fatal_detail; }
extern "C" bool system_safe_mode_boot_recovery_pending(void) { return g_safe_mode_boot_recovery_pending; }
extern "C" SystemInitStage system_safe_mode_boot_recovery_stage(void) { return g_safe_mode_boot_recovery_stage; }
extern "C" bool system_runtime_battery_adc_available(void) { return g_runtime_caps.has_battery_adc; }
extern "C" bool system_runtime_watchdog_available(void) { return g_runtime_caps.has_watchdog; }

extern "C" void system_board_peripheral_init(void)
{
    if (!MX_GPIO_Init()) {
        return;
    }
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_GPIO);
    system_record_stage(SYSTEM_INIT_STAGE_GPIO);

    if (!MX_BKP_Init()) {
        return;
    }
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_BKP);
    system_record_stage(SYSTEM_INIT_STAGE_BKP);

    if (!MX_RTC_Init()) {
        return;
    }
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_RTC);
    system_record_stage(SYSTEM_INIT_STAGE_RTC);

    if (!MX_I2C1_Init()) {
        return;
    }
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_I2C);
    system_record_stage(SYSTEM_INIT_STAGE_I2C);

#if APP_FEATURE_BATTERY
    if (!MX_ADC1_Init()) {
        return;
    }
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_ADC);
    system_record_stage(SYSTEM_INIT_STAGE_ADC);
#endif

#if APP_FEATURE_WATCHDOG
    if (!MX_IWDG_Init()) {
        return;
    }
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_IWDG);
    system_record_stage(SYSTEM_INIT_STAGE_IWDG);
#endif
}

extern "C" void system_runtime_service_init(void)
{
    time_service_init();
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_TIME_SERVICE);
    system_record_stage(SYSTEM_INIT_STAGE_TIME_SERVICE);

    network_sync_service_init();

    storage_service_init();
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_STORAGE_SERVICE);
    system_record_stage(SYSTEM_INIT_STAGE_STORAGE_SERVICE);
}

extern "C" void system_mark_companion_transport_initialized(void)
{
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_COMPANION_TRANSPORT);
    system_record_stage(SYSTEM_INIT_STAGE_COMPANION_TRANSPORT);
}

extern "C" void system_mark_app_initialized(void)
{
    system_init_completion_mark(&g_init_completed, SYSTEM_INIT_STAGE_APP);
    system_record_stage(SYSTEM_INIT_STAGE_APP);
}

extern "C" SystemInitStage system_init_last_stage(void) { return g_last_init_stage; }
extern "C" bool system_init_stage_completed(SystemInitStage stage) { return system_init_completion_is_marked(&g_init_completed, stage); }

extern "C" const char *system_init_stage_name(SystemInitStage stage)
{
    switch (stage) {
        case SYSTEM_INIT_STAGE_NONE: return "NONE";
        case SYSTEM_INIT_STAGE_HAL: return "HAL";
        case SYSTEM_INIT_STAGE_RESET_REASON: return "RESET_REASON";
        case SYSTEM_INIT_STAGE_CLOCK: return "CLOCK";
        case SYSTEM_INIT_STAGE_GPIO: return "GPIO";
        case SYSTEM_INIT_STAGE_UART: return "UART";
        case SYSTEM_INIT_STAGE_BKP: return "BKP";
        case SYSTEM_INIT_STAGE_RTC: return "RTC";
        case SYSTEM_INIT_STAGE_I2C: return "I2C";
        case SYSTEM_INIT_STAGE_ADC: return "ADC";
        case SYSTEM_INIT_STAGE_IWDG: return "IWDG";
        case SYSTEM_INIT_STAGE_CAPABILITY_PROBE: return "CAPS";
        case SYSTEM_INIT_STAGE_TIME_SERVICE: return "TIME";
        case SYSTEM_INIT_STAGE_STORAGE_SERVICE: return "STORAGE";
        case SYSTEM_INIT_STAGE_COMPANION_TRANSPORT: return "COMPANION";
        case SYSTEM_INIT_STAGE_APP: return "APP";
        default: return "UNKNOWN";
    }
}

extern "C" void SystemClock_Config(void) {}

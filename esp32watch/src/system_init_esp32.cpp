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
#include "web/web_server.h"
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
static SystemStartupReport g_startup_report;
static SystemInitStageStatus g_stage_status[SYSTEM_INIT_STAGE_APP + 1U];

extern "C" PlatformI2cBus platform_i2c_main;
extern "C" PlatformRtcDevice platform_rtc_main;
extern "C" PlatformAdcDevice platform_adc_main;
extern "C" PlatformUartPort platform_uart_main;
extern "C" PlatformWatchdog platform_watchdog_main;

static void system_record_stage(SystemInitStage stage)
{
    g_last_init_stage = stage;
    if (stage != SYSTEM_INIT_STAGE_NONE) {
        g_startup_report.last_completed_stage = stage;
    }
}

static void system_set_stage_status(SystemInitStage stage, SystemInitStageStatus status)
{
    if (stage > SYSTEM_INIT_STAGE_NONE && stage <= SYSTEM_INIT_STAGE_APP) {
        g_stage_status[(uint32_t)stage] = status;
    }
}

static void system_mark_stage_completed_internal(SystemInitStage stage)
{
    system_init_completion_mark(&g_init_completed, stage);
    system_set_stage_status(stage, SYSTEM_INIT_STAGE_STATUS_OK);
    system_record_stage(stage);
}

static void system_mark_stage_degraded_internal(SystemInitStage stage)
{
    system_set_stage_status(stage, SYSTEM_INIT_STAGE_STATUS_DEGRADED);
    g_startup_report.degraded = true;
    g_startup_report.recovery_stage = stage;
    system_record_stage(stage);
}

static void system_mark_stage_failed_internal(SystemInitStage stage)
{
    system_set_stage_status(stage, SYSTEM_INIT_STAGE_STATUS_FAILED);
    g_startup_report.startup_ok = false;
    g_startup_report.failure_stage = stage;
}

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
    system_mark_stage_degraded_internal(stage);
}

static bool system_handle_stage_failure(SystemInitStage stage, uint32_t detail, SystemFatalPolicy preferred_policy)
{
    SystemFatalPolicy policy = preferred_policy;

    if (policy == SYSTEM_FATAL_POLICY_SAFE_MODE && !system_init_stage_supports_same_boot_safe_mode(stage)) {
        policy = SYSTEM_FATAL_POLICY_STOP;
    }

    system_raise_fatal(SYSTEM_FATAL_CODE_INIT_STAGE_FAILURE, stage, detail, policy);
    return !g_init_failed;
}

extern "C" void system_raise_fatal(SystemFatalCode code, SystemInitStage stage, uint32_t detail, SystemFatalPolicy policy)
{
    g_last_fatal_code = code;
    g_last_fatal_policy = policy;
    g_last_fatal_detail = detail;
    g_startup_report.fatal_code = code;
    g_startup_report.fatal_policy = policy;
    g_startup_report.fatal_detail = detail;
    crash_capsule_note_init_failure((uint8_t)stage, (uint8_t)policy);

    if (policy == SYSTEM_FATAL_POLICY_SAFE_MODE && system_init_stage_supports_same_boot_safe_mode(stage)) {
        system_apply_same_boot_recovery(stage);
        return;
    }

    system_mark_stage_failed_internal(stage);
    g_startup_report.fatal_stop_requested = true;
    g_init_failed = true;
    system_record_stage(stage);
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
    memset(&g_stage_status, 0, sizeof(g_stage_status));
    memset(&g_startup_report, 0, sizeof(g_startup_report));
    g_startup_report.startup_ok = true;
    system_init_completion_reset(&g_init_completed);
    platform_init();
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_HAL);
    reset_reason_init();
    crash_capsule_init(reset_reason_get());
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_RESET_REASON);
    SystemClock_Config();
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_CLOCK);
}

extern "C" bool system_runtime_capability_probe(SystemRuntimeCapabilities *out)
{
    const BoardManifest *manifest = board_manifest_get();

    memset(&g_runtime_caps, 0, sizeof(g_runtime_caps));
    if (manifest == NULL) {
        system_set_stage_status(SYSTEM_INIT_STAGE_CAPABILITY_PROBE, SYSTEM_INIT_STAGE_STATUS_FAILED);
        g_startup_report.startup_ok = false;
        g_startup_report.failure_stage = SYSTEM_INIT_STAGE_CAPABILITY_PROBE;
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

    bool valid = manifest->profile_name != NULL &&
                 system_init_stage_completed(SYSTEM_INIT_STAGE_GPIO) &&
                 system_init_stage_completed(SYSTEM_INIT_STAGE_BKP) &&
                 system_init_stage_completed(SYSTEM_INIT_STAGE_RTC) &&
                 system_init_stage_completed(SYSTEM_INIT_STAGE_I2C) &&
                 (!manifest->sensor_enabled || (manifest->i2c_port != NULL && manifest->i2c_scl_pin != 0U && manifest->i2c_sda_pin != 0U)) &&
                 (!manifest->vibration_enabled || (manifest->vibe_port != NULL && manifest->vibe_pin != 0U)) &&
                 (!manifest->buzzer_enabled || manifest->buzzer_gpio >= 0) &&
                 g_runtime_caps.storage_map_valid;

    if (valid) {
        system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_CAPABILITY_PROBE);
    } else {
        system_set_stage_status(SYSTEM_INIT_STAGE_CAPABILITY_PROBE, SYSTEM_INIT_STAGE_STATUS_FAILED);
        g_startup_report.startup_ok = false;
        g_startup_report.failure_stage = SYSTEM_INIT_STAGE_CAPABILITY_PROBE;
    }
    return valid;
}

extern "C" bool system_init_has_failed(void) { return g_init_failed; }
extern "C" SystemFatalCode system_last_fatal_code(void) { return g_last_fatal_code; }
extern "C" SystemFatalPolicy system_last_fatal_policy(void) { return g_last_fatal_policy; }
extern "C" uint32_t system_last_fatal_detail(void) { return g_last_fatal_detail; }
extern "C" bool system_safe_mode_boot_recovery_pending(void) { return g_safe_mode_boot_recovery_pending; }
extern "C" SystemInitStage system_safe_mode_boot_recovery_stage(void) { return g_safe_mode_boot_recovery_stage; }
extern "C" bool system_runtime_battery_adc_available(void) { return g_runtime_caps.has_battery_adc; }
extern "C" bool system_runtime_watchdog_available(void) { return g_runtime_caps.has_watchdog; }

extern "C" bool system_board_peripheral_init(void)
{
    if (!MX_GPIO_Init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_GPIO, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_GPIO);

    if (!MX_BKP_Init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_BKP, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_BKP);

    if (!MX_RTC_Init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_RTC, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_RTC);

    if (!MX_I2C1_Init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_I2C, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_I2C);

#if APP_FEATURE_BATTERY
    if (!MX_ADC1_Init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_ADC, 0U, SYSTEM_FATAL_POLICY_SAFE_MODE);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_ADC);
#endif

#if APP_FEATURE_WATCHDOG
    if (!MX_IWDG_Init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_IWDG, 0U, SYSTEM_FATAL_POLICY_SAFE_MODE);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_IWDG);
#endif
    return true;
}

extern "C" bool system_runtime_service_init(void)
{
    if (!time_service_init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_TIME_SERVICE, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_TIME_SERVICE);

    if (!network_sync_service_init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE);

    if (!storage_service_init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_STORAGE_SERVICE, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_STORAGE_SERVICE);
    return true;
}

extern "C" bool system_web_service_init(void)
{
    if (!web_server_init()) {
        return system_handle_stage_failure(SYSTEM_INIT_STAGE_WEB_SERVER, 0U, SYSTEM_FATAL_POLICY_STOP);
    }
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_WEB_SERVER);
    return true;
}

extern "C" void system_mark_companion_transport_initialized(void)
{
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_COMPANION_TRANSPORT);
}

extern "C" void system_mark_app_initialized(void)
{
    system_mark_stage_completed_internal(SYSTEM_INIT_STAGE_APP);
}

extern "C" SystemInitStage system_init_last_stage(void) { return g_last_init_stage; }
extern "C" bool system_init_stage_completed(SystemInitStage stage) { return system_init_completion_is_marked(&g_init_completed, stage); }
extern "C" SystemInitStageStatus system_init_stage_status(SystemInitStage stage)
{
    if (stage <= SYSTEM_INIT_STAGE_NONE || stage > SYSTEM_INIT_STAGE_APP) {
        return SYSTEM_INIT_STAGE_STATUS_NOT_RUN;
    }
    return g_stage_status[(uint32_t)stage];
}

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
        case SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE: return "NETWORK_SYNC";
        case SYSTEM_INIT_STAGE_STORAGE_SERVICE: return "STORAGE";
        case SYSTEM_INIT_STAGE_WEB_SERVER: return "WEB";
        case SYSTEM_INIT_STAGE_COMPANION_TRANSPORT: return "COMPANION";
        case SYSTEM_INIT_STAGE_APP: return "APP";
        default: return "UNKNOWN";
    }
}

extern "C" const char *system_init_stage_status_name(SystemInitStageStatus status)
{
    switch (status) {
        case SYSTEM_INIT_STAGE_STATUS_OK: return "OK";
        case SYSTEM_INIT_STAGE_STATUS_DEGRADED: return "DEGRADED";
        case SYSTEM_INIT_STAGE_STATUS_FAILED: return "FAILED";
        case SYSTEM_INIT_STAGE_STATUS_NOT_RUN:
        default:
            return "NOT_RUN";
    }
}

extern "C" const char *system_fatal_code_name(SystemFatalCode code)
{
    switch (code) {
        case SYSTEM_FATAL_CODE_NONE: return "NONE";
        case SYSTEM_FATAL_CODE_INIT_STAGE_FAILURE: return "INIT_STAGE_FAILURE";
        case SYSTEM_FATAL_CODE_CAPABILITY_CONTRACT: return "CAPABILITY_CONTRACT";
        default: return "UNKNOWN";
    }
}

extern "C" const char *system_fatal_policy_name(SystemFatalPolicy policy)
{
    switch (policy) {
        case SYSTEM_FATAL_POLICY_SAFE_MODE: return "SAFE_MODE";
        case SYSTEM_FATAL_POLICY_STOP:
        default:
            return "STOP";
    }
}

extern "C" bool system_get_startup_report(SystemStartupReport *out)
{
    if (out == NULL) {
        return false;
    }
    *out = g_startup_report;
    return true;
}


extern "C" bool system_get_startup_status(SystemStartupStatus *out)
{
    if (out == NULL) {
        return false;
    }
    out->init_failed = g_init_failed;
    out->last_stage = g_last_init_stage;
    out->safe_mode_boot_recovery_pending = g_safe_mode_boot_recovery_pending;
    out->safe_mode_boot_recovery_stage = g_safe_mode_boot_recovery_stage;
    out->fatal_code = g_last_fatal_code;
    out->fatal_policy = g_last_fatal_policy;
    out->fatal_detail = g_last_fatal_detail;
    return true;
}

extern "C" void SystemClock_Config(void) {}

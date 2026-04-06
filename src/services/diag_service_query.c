#include "services/diag_service_internal.h"
#include "crash_capsule.h"
#include "reset_reason.h"
#include <string.h>

uint16_t diag_service_get_log_count(void)
{
    return ringbuf_count(&g_diag.rb);
}

uint32_t diag_service_get_log_overflow_count(void)
{
    return ringbuf_get_overflow_count(&g_diag.rb);
}

bool diag_service_get_last_log(DiagLogEntry *out)
{
    if (!g_diag.has_last || out == NULL) {
        return false;
    }
    *out = g_diag.last;
    return true;
}

const char *diag_service_event_name(DiagEventCode code)
{
    switch (code) {
        case DIAG_EVT_BOOT: return "BOOT";
        case DIAG_EVT_WAKE: return "WAKE";
        case DIAG_EVT_SLEEP: return "SLEEP";
        case DIAG_EVT_STORAGE_COMMIT: return "STOR";
        case DIAG_EVT_STORAGE_FAIL: return "SFAIL";
        case DIAG_EVT_SENSOR_ONLINE: return "IMUON";
        case DIAG_EVT_SENSOR_FAULT: return "IMUFL";
        case DIAG_EVT_DISPLAY_TX_FAIL: return "OLED";
        case DIAG_EVT_KEY_OVERFLOW: return "KEYOV";
        case DIAG_EVT_UI_MUTATION_OVERFLOW: return "UIQOV";
        case DIAG_EVT_BATTERY_SAMPLE: return "BATT";
        case DIAG_EVT_WDT_INIT: return "WDTI";
        case DIAG_EVT_WDT_PET: return "WDTP";
        case DIAG_EVT_WDT_STARVED: return "WDTS";
        default: return "NONE";
    }
}

uint32_t diag_service_get_storage_fail_count(void)
{
    return g_diag.storage_fail_count;
}

uint32_t diag_service_get_sensor_fault_event_count(void)
{
    return g_diag.sensor_fault_event_count;
}

uint32_t diag_service_get_display_tx_fail_count(void)
{
    return g_diag.display_tx_fail_count;
}

uint32_t diag_service_get_key_overflow_event_count(void)
{
    return g_diag.key_overflow_event_count;
}

uint32_t diag_service_get_ui_mutation_overflow_event_count(void)
{
    return g_diag.ui_mutation_overflow_event_count;
}

uint32_t diag_service_get_battery_sample_count(void)
{
    return g_diag.battery_sample_count;
}

uint32_t diag_service_get_wdt_pet_count(void)
{
    return g_diag.wdt_pet_count;
}

uint16_t diag_service_get_last_retained_checkpoint(void)
{
    return crash_capsule_has_previous() ? crash_capsule_get_previous()->last_checkpoint : 0U;
}

uint16_t diag_service_get_last_retained_fault_value(void)
{
    return crash_capsule_has_previous() ? crash_capsule_get_previous()->last_fault_value : 0U;
}

uint8_t diag_service_get_last_retained_fault_code(void)
{
    return crash_capsule_has_previous() ? crash_capsule_get_previous()->last_fault_code : 0U;
}

uint32_t diag_service_get_retained_max_loop_ms(void)
{
    return crash_capsule_has_previous() ? crash_capsule_get_previous()->max_loop_ms : 0U;
}

bool diag_service_export_snapshot(DiagExportSnapshot *out)
{
    uint8_t i;

    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->safe_mode_active = g_diag.safe_mode_active;
    out->safe_mode_can_clear = diag_service_can_clear_safe_mode();
    out->storage_fail_count = g_diag.storage_fail_count;
    out->sensor_fault_event_count = g_diag.sensor_fault_event_count;
    out->display_tx_fail_count = g_diag.display_tx_fail_count;
    out->key_overflow_event_count = g_diag.key_overflow_event_count;
    out->ui_mutation_overflow_event_count = g_diag.ui_mutation_overflow_event_count;
    out->battery_sample_count = g_diag.battery_sample_count;
    out->wdt_pet_count = g_diag.wdt_pet_count;
    out->previous_boot_incomplete = crash_capsule_has_previous() &&
                                    crash_capsule_get_previous()->boot_in_progress != 0U;
    out->previous_init_failed_stage = crash_capsule_has_previous() ?
                                      crash_capsule_get_previous()->init_failed_stage : 0U;
    for (i = (uint8_t)DIAG_FAULT_NONE + 1U; i <= (uint8_t)DIAG_FAULT_WDT_RESET; ++i) {
        if (g_diag.faults[i].seen) {
            out->active_fault_count++;
            if (g_diag.faults[i].blocking) {
                out->blocking_fault_count++;
            }
        }
    }
    return true;
}

bool diag_service_get_fault_info(DiagFaultCode code, DiagFaultInfo *out)
{
    if (out == NULL || code <= DIAG_FAULT_NONE || code > DIAG_FAULT_WDT_RESET) {
        return false;
    }
    if (!g_diag.faults[code].seen) {
        return false;
    }
    *out = g_diag.faults[code];
    return true;
}

bool diag_service_get_last_fault(DiagFaultCode *code, DiagFaultInfo *out)
{
    if (!g_diag.has_last_fault) {
        return false;
    }
    if (code != NULL) {
        *code = g_diag.last_fault_code;
    }
    if (out != NULL) {
        *out = g_diag.faults[g_diag.last_fault_code];
    }
    return true;
}

const char *diag_service_fault_name(DiagFaultCode code)
{
    switch (code) {
        case DIAG_FAULT_STORAGE_COMMIT: return "STOR";
        case DIAG_FAULT_SENSOR: return "SENS";
        case DIAG_FAULT_DISPLAY_TX: return "OLED";
        case DIAG_FAULT_RTC_INVALID: return "RTC";
        case DIAG_FAULT_BATTERY_ADC: return "BATT";
        case DIAG_FAULT_WDT_RESET: return "WDT";
        default: return "NONE";
    }
}

const char *diag_service_fault_owner_name(DiagFaultOwner owner)
{
    switch (owner) {
        case DIAG_FAULT_OWNER_STORAGE: return "STOR";
        case DIAG_FAULT_OWNER_SENSOR: return "SENS";
        case DIAG_FAULT_OWNER_DISPLAY: return "OLED";
        case DIAG_FAULT_OWNER_RTC: return "RTC";
        case DIAG_FAULT_OWNER_BATTERY: return "BATT";
        case DIAG_FAULT_OWNER_WDT: return "WDT";
        default: return "NONE";
    }
}

const char *diag_service_fault_lifecycle_name(DiagFaultLifecycle lifecycle)
{
    switch (lifecycle) {
        case DIAG_FAULT_LIFECYCLE_RETRYING: return "RETRY";
        case DIAG_FAULT_LIFECYCLE_DEGRADED: return "DEGR";
        case DIAG_FAULT_LIFECYCLE_LATCHED: return "LATCH";
        case DIAG_FAULT_LIFECYCLE_RECOVERED: return "RECOV";
        default: return "NONE";
    }
}

const char *diag_service_fault_severity_name(DiagFaultSeverity severity)
{
    switch (severity) {
        case DIAG_FAULT_SEVERITY_RECOVERABLE: return "RECOV";
        case DIAG_FAULT_SEVERITY_DEGRADED: return "DEGR";
        case DIAG_FAULT_SEVERITY_FATAL: return "FATAL";
        default: return "NONE";
    }
}

bool diag_service_is_safe_mode_active(void)
{
    return g_diag.safe_mode_active;
}

DiagSafeModeReason diag_service_get_safe_mode_reason(void)
{
    return g_diag.safe_mode_reason;
}

const char *diag_service_safe_mode_reason_name(DiagSafeModeReason reason)
{
    switch (reason) {
        case DIAG_SAFE_MODE_WDT_RESET: return "WDT";
        case DIAG_SAFE_MODE_STORAGE_FAULT: return "STOR";
        case DIAG_SAFE_MODE_SENSOR_FAULT: return "SENS";
        case DIAG_SAFE_MODE_INIT_FAILURE: return "INIT";
        default: return "NONE";
    }
}

bool diag_service_can_clear_safe_mode(void)
{
    return !g_diag.safe_mode_active || g_diag.safe_mode_clear_allowed;
}

const char *diag_service_reset_reason_name(ResetReason reason)
{
    return reset_reason_name(reason);
}

const char *diag_service_wake_reason_name(WakeReason reason)
{
    switch (reason) {
        case WAKE_REASON_BOOT: return "BOOT";
        case WAKE_REASON_KEY: return "KEY";
        case WAKE_REASON_POPUP: return "POP";
        case WAKE_REASON_WRIST_RAISE: return "WRST";
        case WAKE_REASON_MANUAL: return "MAN";
        case WAKE_REASON_SERVICE: return "SRV";
        default: return "NONE";
    }
}

const char *diag_service_sleep_reason_name(SleepReason reason)
{
    switch (reason) {
        case SLEEP_REASON_AUTO_TIMEOUT: return "AUTO";
        case SLEEP_REASON_MANUAL: return "MAN";
        case SLEEP_REASON_SERVICE: return "SRV";
        default: return "NONE";
    }
}

const char *diag_service_storage_commit_reason_name(StorageCommitReason reason)
{
    switch (reason) {
        case STORAGE_COMMIT_REASON_IDLE: return "IDLE";
        case STORAGE_COMMIT_REASON_MAX_AGE: return "MAX";
        case STORAGE_COMMIT_REASON_CALIBRATION: return "CAL";
        case STORAGE_COMMIT_REASON_MANUAL: return "MAN";
        case STORAGE_COMMIT_REASON_SLEEP: return "SLP";
        case STORAGE_COMMIT_REASON_RESTORE_DEFAULTS: return "RST";
        case STORAGE_COMMIT_REASON_ALARM: return "ALRM";
        default: return "NONE";
    }
}

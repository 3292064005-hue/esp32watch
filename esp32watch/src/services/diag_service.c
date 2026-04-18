#include "services/diag_service.h"
#include "services/diag_service_internal.h"
#include "crash_capsule.h"
#include "reset_reason.h"
#include "system_init.h"
#include "platform_api.h"
#include "app_limits.h"
#include <string.h>

#define DIAG_SENSOR_SAFE_MODE_THRESHOLD 8U
#define DIAG_FAULT_RECOVERY_WINDOW_MS 3000U
#define DIAG_SAFE_MODE_CLEAR_HOLDOFF_MS 3000U

DiagServiceState g_diag;

static uint16_t diag_fault_cooldown_ms(DiagFaultCode code)
{
    switch (code) {
        case DIAG_FAULT_STORAGE_COMMIT: return 4000U;
        case DIAG_FAULT_SENSOR: return 5000U;
        case DIAG_FAULT_DISPLAY_TX:
        case DIAG_FAULT_TIME_INVALID:
        case DIAG_FAULT_BATTERY_ADC: return 2000U;
        case DIAG_FAULT_WDT_RESET: return DIAG_SAFE_MODE_CLEAR_HOLDOFF_MS;
        default: return 0U;
    }
}

static DiagFaultOwner diag_fault_owner(DiagFaultCode code)
{
    switch (code) {
        case DIAG_FAULT_STORAGE_COMMIT: return DIAG_FAULT_OWNER_STORAGE;
        case DIAG_FAULT_SENSOR: return DIAG_FAULT_OWNER_SENSOR;
        case DIAG_FAULT_DISPLAY_TX: return DIAG_FAULT_OWNER_DISPLAY;
        case DIAG_FAULT_TIME_INVALID: return DIAG_FAULT_OWNER_TIME;
        case DIAG_FAULT_BATTERY_ADC: return DIAG_FAULT_OWNER_BATTERY;
        case DIAG_FAULT_WDT_RESET: return DIAG_FAULT_OWNER_WDT;
        default: return DIAG_FAULT_OWNER_NONE;
    }
}

static uint8_t diag_fault_retry_budget(DiagFaultCode code)
{
    switch (code) {
        case DIAG_FAULT_STORAGE_COMMIT: return 2U;
        case DIAG_FAULT_SENSOR: return 3U;
        case DIAG_FAULT_DISPLAY_TX:
        case DIAG_FAULT_TIME_INVALID:
        case DIAG_FAULT_BATTERY_ADC: return 1U;
        case DIAG_FAULT_WDT_RESET: return 0U;
        default: return 0U;
    }
}

DiagFaultSeverity diag_service_fault_severity(DiagFaultCode code)
{
    switch (code) {
        case DIAG_FAULT_STORAGE_COMMIT:
        case DIAG_FAULT_SENSOR:
            return DIAG_FAULT_SEVERITY_DEGRADED;
        case DIAG_FAULT_WDT_RESET:
            return DIAG_FAULT_SEVERITY_FATAL;
        case DIAG_FAULT_DISPLAY_TX:
        case DIAG_FAULT_TIME_INVALID:
        case DIAG_FAULT_BATTERY_ADC:
            return DIAG_FAULT_SEVERITY_RECOVERABLE;
        default:
            return DIAG_FAULT_SEVERITY_NONE;
    }
}

static DiagFaultLifecycle diag_fault_default_lifecycle(DiagFaultCode code, uint8_t count)
{
    DiagFaultSeverity severity = diag_service_fault_severity(code);
    uint8_t retry_budget = diag_fault_retry_budget(code);

    if (severity == DIAG_FAULT_SEVERITY_FATAL) {
        return DIAG_FAULT_LIFECYCLE_LATCHED;
    }
    if (count == 0U) {
        return DIAG_FAULT_LIFECYCLE_NONE;
    }
    if (retry_budget != 0U && count < retry_budget) {
        return DIAG_FAULT_LIFECYCLE_RETRYING;
    }
    return DIAG_FAULT_LIFECYCLE_DEGRADED;
}

static bool diag_fault_is_blocking(const DiagFaultInfo *info, uint32_t now_ms)
{
    if (info == NULL || !info->seen || info->severity == DIAG_FAULT_SEVERITY_NONE) {
        return false;
    }
    if (info->lifecycle == DIAG_FAULT_LIFECYCLE_RECOVERED) {
        return now_ms < info->cooldown_until_ms;
    }
    if (info->owner == DIAG_FAULT_OWNER_WDT) {
        return now_ms < info->cooldown_until_ms;
    }
    return now_ms < info->cooldown_until_ms || info->lifecycle == DIAG_FAULT_LIFECYCLE_LATCHED;
}

static void diag_service_note_fault_internal(DiagFaultCode code, uint16_t value, uint8_t aux)
{
    DiagFaultInfo *info;
    uint32_t now;

    if (code <= DIAG_FAULT_NONE || code > DIAG_FAULT_WDT_RESET) {
        return;
    }

    now = platform_time_now_ms();
    info = &g_diag.faults[code];
    if (!info->seen) {
        memset(info, 0, sizeof(*info));
        info->first_seen_ms = now;
        info->seen = true;
    }
    info->last_seen_ms = now;
    info->last_value = value;
    info->last_aux = aux;
    info->owner = diag_fault_owner(code);
    info->severity = diag_service_fault_severity(code);
    info->retry_budget = diag_fault_retry_budget(code);
    info->cooldown_ms = diag_fault_cooldown_ms(code);
    info->cooldown_until_ms = now + info->cooldown_ms;
    if (info->count < 0xFFU) {
        info->count++;
    }
    info->lifecycle = diag_fault_default_lifecycle(code, info->count);
    info->blocking = true;
    g_diag.last_fault_code = code;
    g_diag.has_last_fault = true;
    g_diag.safe_mode_clear_allowed = false;
    crash_capsule_note_fault_with_severity((uint8_t)code, value, aux, (uint8_t)info->severity);
}

static void diag_service_mark_fault_recovered(DiagFaultCode code)
{
    DiagFaultInfo *info;

    if (code <= DIAG_FAULT_NONE || code > DIAG_FAULT_WDT_RESET) {
        return;
    }

    info = &g_diag.faults[code];
    if (!info->seen) {
        return;
    }
    info->lifecycle = DIAG_FAULT_LIFECYCLE_RECOVERED;
    info->recovered_at_ms = platform_time_now_ms();
    info->cooldown_ms = diag_fault_cooldown_ms(code);
    info->cooldown_until_ms = info->recovered_at_ms + info->cooldown_ms;
    info->blocking = info->cooldown_ms != 0U;
    if (info->recovery_count < 0xFFU) {
        info->recovery_count++;
    }
}

static void diag_service_request_safe_mode(DiagSafeModeReason reason, DiagFaultCode cause)
{
    if (reason == DIAG_SAFE_MODE_NONE) {
        return;
    }
    if (!g_diag.safe_mode_active) {
        g_diag.safe_mode_since_ms = platform_time_now_ms();
    }
    g_diag.safe_mode_active = true;
    g_diag.safe_mode_clear_allowed = false;
    g_diag.safe_mode_reason = reason;
    if (cause > DIAG_FAULT_NONE && cause <= DIAG_FAULT_WDT_RESET && g_diag.faults[cause].seen) {
        g_diag.faults[cause].lifecycle = DIAG_FAULT_LIFECYCLE_LATCHED;
    }
}

void diag_service_init(void)
{
    const CrashCapsuleSnapshot *previous;

    memset(&g_diag, 0, sizeof(g_diag));
    ringbuf_init(&g_diag.rb, g_diag.storage, DIAG_LOG_CAPACITY, sizeof(DiagLogEntry));
    crash_capsule_init(reset_reason_get());

    previous = crash_capsule_has_previous() ? crash_capsule_get_previous() : NULL;
    if (system_safe_mode_boot_recovery_pending()) {
        diag_service_request_safe_mode(DIAG_SAFE_MODE_INIT_FAILURE, DIAG_FAULT_NONE);
    } else if (crash_capsule_consecutive_incomplete_boots() >= APP_BOOT_LOOP_SAFE_MODE_THRESHOLD) {
        diag_service_request_safe_mode(DIAG_SAFE_MODE_BOOT_LOOP, DIAG_FAULT_NONE);
    } else if (previous != NULL &&
               previous->init_failed_stage != 0U &&
               previous->init_failed_policy == (uint8_t)SYSTEM_FATAL_POLICY_SAFE_MODE) {
        diag_service_request_safe_mode(DIAG_SAFE_MODE_INIT_FAILURE, DIAG_FAULT_NONE);
    }
}

void diag_service_tick(uint32_t now_ms)
{
    bool blocking = false;
    uint8_t i;

    for (i = (uint8_t)DIAG_FAULT_NONE + 1U; i <= (uint8_t)DIAG_FAULT_WDT_RESET; ++i) {
        DiagFaultInfo *info = &g_diag.faults[i];

        if (info->seen && info->lifecycle == DIAG_FAULT_LIFECYCLE_RECOVERED && now_ms >= info->cooldown_until_ms) {
            info->blocking = false;
        }
        if (info->seen && info->lifecycle != DIAG_FAULT_LIFECYCLE_RECOVERED && info->severity == DIAG_FAULT_SEVERITY_RECOVERABLE && now_ms >= info->cooldown_until_ms) {
            diag_service_mark_fault_recovered((DiagFaultCode)i);
        }
        info->blocking = diag_fault_is_blocking(info, now_ms);
        if (info->blocking) {
            blocking = true;
            break;
        }
    }

    if (!g_diag.safe_mode_active) {
        g_diag.safe_mode_clear_allowed = false;
        return;
    }

    g_diag.safe_mode_clear_allowed = !blocking &&
                                     ((now_ms - g_diag.safe_mode_since_ms) >= DIAG_SAFE_MODE_CLEAR_HOLDOFF_MS);
}

void diag_service_log(DiagEventCode code, uint16_t value, uint8_t aux)
{
    DiagLogEntry entry;
    entry.timestamp_ms = platform_time_now_ms();
    entry.value = value;
    entry.code = (uint8_t)code;
    entry.aux = aux;
    (void)ringbuf_push(&g_diag.rb, &entry);
    g_diag.last = entry;
    g_diag.has_last = true;
}

void diag_service_note_boot(ResetReason reason)
{
    diag_service_log(DIAG_EVT_BOOT, (uint16_t)reason, 0U);
    if (reason == RESET_REASON_IWDG || reason == RESET_REASON_WWDG) {
        diag_service_note_fault_internal(DIAG_FAULT_WDT_RESET, (uint16_t)reason, 0U);
        diag_service_request_safe_mode(DIAG_SAFE_MODE_WDT_RESET, DIAG_FAULT_WDT_RESET);
    }
}

void diag_service_note_wake(WakeReason reason)
{
    diag_service_log(DIAG_EVT_WAKE, (uint16_t)reason, 0U);
}

void diag_service_note_sleep(SleepReason reason)
{
    diag_service_log(DIAG_EVT_SLEEP, (uint16_t)reason, 0U);
}

void diag_service_note_storage_commit(StorageCommitReason reason, bool ok, uint32_t commit_count)
{
    crash_capsule_note_storage(commit_count, 0U);
    if (!ok) {
        g_diag.storage_fail_count++;
        diag_service_note_fault_internal(DIAG_FAULT_STORAGE_COMMIT,
                                         (uint16_t)(commit_count > 0xFFFFU ? 0xFFFFU : commit_count),
                                         (uint8_t)reason);
        if (reason != STORAGE_COMMIT_REASON_IDLE) {
            diag_service_request_safe_mode(DIAG_SAFE_MODE_STORAGE_FAULT, DIAG_FAULT_STORAGE_COMMIT);
        }
    } else {
        diag_service_mark_fault_recovered(DIAG_FAULT_STORAGE_COMMIT);
    }
    diag_service_log(ok ? DIAG_EVT_STORAGE_COMMIT : DIAG_EVT_STORAGE_FAIL,
                     (uint16_t)(commit_count > 0xFFFFU ? 0xFFFFU : commit_count),
                     (uint8_t)reason);
}

void diag_service_note_sensor_online(uint8_t reinit_count)
{
    crash_capsule_note_sensor(0U, reinit_count);
    diag_service_mark_fault_recovered(DIAG_FAULT_SENSOR);
    diag_service_log(DIAG_EVT_SENSOR_ONLINE, reinit_count, 0U);
}

void diag_service_note_sensor_fault(uint8_t error_code, uint8_t fault_count)
{
    g_diag.sensor_fault_event_count++;
    crash_capsule_note_sensor(error_code, 0U);
    diag_service_note_fault_internal(DIAG_FAULT_SENSOR, error_code, fault_count);
    if (fault_count >= DIAG_SENSOR_SAFE_MODE_THRESHOLD) {
        diag_service_request_safe_mode(DIAG_SAFE_MODE_SENSOR_FAULT, DIAG_FAULT_SENSOR);
    }
    diag_service_log(DIAG_EVT_SENSOR_FAULT, error_code, fault_count);
}

void diag_service_note_display_tx_fail(uint16_t status)
{
    g_diag.display_tx_fail_count++;
    diag_service_note_fault_internal(DIAG_FAULT_DISPLAY_TX, status, 0U);
    diag_service_log(DIAG_EVT_DISPLAY_TX_FAIL, status, 0U);
}

void diag_service_note_key_overflow(uint16_t overflow_count)
{
    g_diag.key_overflow_event_count++;
    diag_service_log(DIAG_EVT_KEY_OVERFLOW, overflow_count, 0U);
}

void diag_service_note_ui_mutation_overflow(uint16_t queued_count)
{
    g_diag.ui_mutation_overflow_event_count++;
    diag_service_log(DIAG_EVT_UI_MUTATION_OVERFLOW, queued_count, 0U);
}

void diag_service_note_battery_sample(uint16_t mv, uint8_t percent)
{
    g_diag.battery_sample_count++;
    diag_service_log(DIAG_EVT_BATTERY_SAMPLE, mv, percent);
}

void diag_service_note_battery_fault(uint16_t status)
{
    diag_service_note_fault_internal(DIAG_FAULT_BATTERY_ADC, status, 0U);
}

void diag_service_note_wdt_init(void)
{
    diag_service_log(DIAG_EVT_WDT_INIT, 0U, 0U);
}

void diag_service_note_wdt_pet(void)
{
    g_diag.wdt_pet_count++;
    if ((g_diag.wdt_pet_count % 64U) == 0U) {
        diag_service_log(DIAG_EVT_WDT_PET,
                         (uint16_t)(g_diag.wdt_pet_count > 0xFFFFU ? 0xFFFFU : g_diag.wdt_pet_count),
                         0U);
    }
}

void diag_service_note_checkpoint_timeout(uint8_t checkpoint, uint16_t loop_ms)
{
    diag_service_note_fault_internal(DIAG_FAULT_WDT_RESET, loop_ms, checkpoint);
    diag_service_log(DIAG_EVT_WDT_STARVED, loop_ms, checkpoint);
}

static void diag_service_record_time_invalid_fault(void)
{
    diag_service_note_fault_internal(DIAG_FAULT_TIME_INVALID, 0U, 0U);
}

void diag_service_note_time_invalid(void)
{
    diag_service_record_time_invalid_fault();
}

void diag_service_clear_safe_mode(void)
{
    if (!diag_service_can_clear_safe_mode()) {
        return;
    }
    g_diag.safe_mode_active = false;
    g_diag.safe_mode_clear_allowed = false;
    g_diag.safe_mode_since_ms = 0U;
    g_diag.safe_mode_reason = DIAG_SAFE_MODE_NONE;
}



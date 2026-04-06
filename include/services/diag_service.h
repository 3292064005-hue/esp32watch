#ifndef DIAG_SERVICE_H
#define DIAG_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

typedef enum {
    DIAG_EVT_NONE = 0,
    DIAG_EVT_BOOT,
    DIAG_EVT_WAKE,
    DIAG_EVT_SLEEP,
    DIAG_EVT_STORAGE_COMMIT,
    DIAG_EVT_STORAGE_FAIL,
    DIAG_EVT_SENSOR_ONLINE,
    DIAG_EVT_SENSOR_FAULT,
    DIAG_EVT_DISPLAY_TX_FAIL,
    DIAG_EVT_KEY_OVERFLOW,
    DIAG_EVT_UI_MUTATION_OVERFLOW,
    DIAG_EVT_BATTERY_SAMPLE,
    DIAG_EVT_WDT_INIT,
    DIAG_EVT_WDT_PET,
    DIAG_EVT_WDT_STARVED
} DiagEventCode;

typedef struct {
    uint32_t timestamp_ms;
    uint16_t value;
    uint8_t code;
    uint8_t aux;
} DiagLogEntry;

typedef enum {
    DIAG_FAULT_NONE = 0,
    DIAG_FAULT_STORAGE_COMMIT,
    DIAG_FAULT_SENSOR,
    DIAG_FAULT_DISPLAY_TX,
    DIAG_FAULT_RTC_INVALID,
    DIAG_FAULT_BATTERY_ADC,
    DIAG_FAULT_WDT_RESET
} DiagFaultCode;

typedef enum {
    DIAG_FAULT_OWNER_NONE = 0,
    DIAG_FAULT_OWNER_STORAGE,
    DIAG_FAULT_OWNER_SENSOR,
    DIAG_FAULT_OWNER_DISPLAY,
    DIAG_FAULT_OWNER_RTC,
    DIAG_FAULT_OWNER_BATTERY,
    DIAG_FAULT_OWNER_WDT
} DiagFaultOwner;

typedef enum {
    DIAG_FAULT_SEVERITY_NONE = 0,
    DIAG_FAULT_SEVERITY_RECOVERABLE,
    DIAG_FAULT_SEVERITY_DEGRADED,
    DIAG_FAULT_SEVERITY_FATAL
} DiagFaultSeverity;

typedef enum {
    DIAG_FAULT_LIFECYCLE_NONE = 0,
    DIAG_FAULT_LIFECYCLE_RETRYING,
    DIAG_FAULT_LIFECYCLE_DEGRADED,
    DIAG_FAULT_LIFECYCLE_LATCHED,
    DIAG_FAULT_LIFECYCLE_RECOVERED
} DiagFaultLifecycle;

typedef enum {
    DIAG_SAFE_MODE_NONE = 0,
    DIAG_SAFE_MODE_WDT_RESET,
    DIAG_SAFE_MODE_STORAGE_FAULT,
    DIAG_SAFE_MODE_SENSOR_FAULT,
    DIAG_SAFE_MODE_INIT_FAILURE
} DiagSafeModeReason;

typedef struct {
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint32_t recovered_at_ms;
    uint16_t last_value;
    uint8_t last_aux;
    uint8_t count;
    uint8_t recovery_count;
    uint8_t retry_budget;
    uint16_t cooldown_ms;
    uint32_t cooldown_until_ms;
    bool blocking;
    DiagFaultOwner owner;
    DiagFaultSeverity severity;
    DiagFaultLifecycle lifecycle;
    bool seen;
} DiagFaultInfo;

typedef struct {
    bool safe_mode_active;
    bool safe_mode_can_clear;
    uint8_t active_fault_count;
    uint8_t blocking_fault_count;
    uint32_t storage_fail_count;
    uint32_t sensor_fault_event_count;
    uint32_t display_tx_fail_count;
    uint32_t key_overflow_event_count;
    uint32_t ui_mutation_overflow_event_count;
    uint32_t battery_sample_count;
    uint32_t wdt_pet_count;
    bool previous_boot_incomplete;
    uint8_t previous_init_failed_stage;
} DiagExportSnapshot;

void diag_service_init(void);
void diag_service_tick(uint32_t now_ms);
void diag_service_log(DiagEventCode code, uint16_t value, uint8_t aux);
void diag_service_note_boot(ResetReason reason);
void diag_service_note_wake(WakeReason reason);
void diag_service_note_sleep(SleepReason reason);
void diag_service_note_storage_commit(StorageCommitReason reason, bool ok, uint32_t commit_count);
void diag_service_note_sensor_online(uint8_t reinit_count);
void diag_service_note_sensor_fault(uint8_t error_code, uint8_t fault_count);
void diag_service_note_display_tx_fail(uint16_t status);
void diag_service_note_key_overflow(uint16_t overflow_count);
void diag_service_note_ui_mutation_overflow(uint16_t queued_count);
void diag_service_note_battery_sample(uint16_t mv, uint8_t percent);
void diag_service_note_wdt_init(void);
void diag_service_note_wdt_pet(void);
void diag_service_note_checkpoint_timeout(uint8_t checkpoint, uint16_t loop_ms);
void diag_service_note_rtc_invalid(void);
void diag_service_note_battery_fault(uint16_t status);

uint16_t diag_service_get_log_count(void);
uint32_t diag_service_get_log_overflow_count(void);
bool diag_service_get_last_log(DiagLogEntry *out);
const char *diag_service_event_name(DiagEventCode code);
uint32_t diag_service_get_storage_fail_count(void);
uint32_t diag_service_get_sensor_fault_event_count(void);
uint32_t diag_service_get_display_tx_fail_count(void);
uint32_t diag_service_get_key_overflow_event_count(void);
uint32_t diag_service_get_ui_mutation_overflow_event_count(void);
uint32_t diag_service_get_battery_sample_count(void);
uint32_t diag_service_get_wdt_pet_count(void);
uint16_t diag_service_get_last_retained_checkpoint(void);
uint16_t diag_service_get_last_retained_fault_value(void);
uint8_t diag_service_get_last_retained_fault_code(void);
uint32_t diag_service_get_retained_max_loop_ms(void);
bool diag_service_export_snapshot(DiagExportSnapshot *out);
bool diag_service_get_fault_info(DiagFaultCode code, DiagFaultInfo *out);
bool diag_service_get_last_fault(DiagFaultCode *code, DiagFaultInfo *out);
const char *diag_service_fault_name(DiagFaultCode code);
DiagFaultSeverity diag_service_fault_severity(DiagFaultCode code);
const char *diag_service_fault_owner_name(DiagFaultOwner owner);
const char *diag_service_fault_lifecycle_name(DiagFaultLifecycle lifecycle);
const char *diag_service_fault_severity_name(DiagFaultSeverity severity);
bool diag_service_is_safe_mode_active(void);
DiagSafeModeReason diag_service_get_safe_mode_reason(void);
const char *diag_service_safe_mode_reason_name(DiagSafeModeReason reason);

/**
 * @brief Query whether the current safe-mode latch satisfies the clear policy.
 *
 * @param None.
 * @return true when safe mode may be cleared without violating the recovery window.
 * @throws None.
 */
bool diag_service_can_clear_safe_mode(void);

void diag_service_clear_safe_mode(void);
const char *diag_service_reset_reason_name(ResetReason reason);
const char *diag_service_wake_reason_name(WakeReason reason);
const char *diag_service_sleep_reason_name(SleepReason reason);
const char *diag_service_storage_commit_reason_name(StorageCommitReason reason);

#endif

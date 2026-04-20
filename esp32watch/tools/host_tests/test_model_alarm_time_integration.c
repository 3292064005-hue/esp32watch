#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "model.h"
#include "model_internal.h"
#include "platform_api.h"
#include "services/time_service.h"
#include "services/storage_service.h"
#include "services/storage_facade.h"
#include "services/sensor_service.h"
#include "services/diag_service.h"
#include "services/power_service.h"
#include "services/input_service.h"
#include "reset_reason.h"

static uint32_t g_now_ms = 0;
static uint32_t g_epoch = 0;
static TimeSourceSnapshot g_snapshot = {0};

uint32_t platform_time_now_ms(void) { return g_now_ms; }
void platform_delay_ms(uint32_t delay_ms) { g_now_ms += delay_ms; }

bool time_service_init(void) { return true; }
TimeInitStatus time_service_init_status(void) { return TIME_INIT_STATUS_READY; }
const char *time_service_init_status_name(TimeInitStatus status) { (void)status; return "READY"; }
const char *time_service_init_reason(void) { return "TEST"; }
bool time_service_is_datetime_valid(const DateTime *dt) { return dt && dt->year >= 2000U && dt->month >= 1U && dt->month <= 12U && dt->day >= 1U && dt->day <= 31U; }

uint32_t time_service_datetime_to_epoch(const DateTime *dt)
{
    return (((uint32_t)dt->day * 24U + dt->hour) * 60U + dt->minute) * 60U + dt->second;
}

void time_service_epoch_to_datetime(uint32_t epoch, DateTime *out)
{
    memset(out, 0, sizeof(*out));
    out->year = 2026U;
    out->month = 4U;
    out->day = (uint8_t)(epoch / 86400U);
    uint32_t sec_of_day = epoch % 86400U;
    out->hour = (uint8_t)(sec_of_day / 3600U);
    out->minute = (uint8_t)((sec_of_day % 3600U) / 60U);
    out->second = (uint8_t)(sec_of_day % 60U);
    out->weekday = (uint8_t)(out->day % 7U);
}

uint32_t time_service_get_epoch(void) { return g_epoch; }
void time_service_refresh(DateTime *out) { if (out) time_service_epoch_to_datetime(g_epoch, out); }
bool time_service_get_datetime_snapshot(DateTime *out, TimeSourceSnapshot *source_out)
{
    if (out) time_service_epoch_to_datetime(g_epoch, out);
    if (source_out) *source_out = g_snapshot;
    return out != NULL || source_out != NULL;
}
bool time_service_set_epoch_from_source(uint32_t epoch, TimeSourceType source)
{
    g_epoch = epoch;
    g_snapshot.source = source;
    g_snapshot.authority = TIME_AUTHORITY_NETWORK;
    g_snapshot.confidence = TIME_CONFIDENCE_HIGH;
    g_snapshot.valid = true;
    g_snapshot.epoch = epoch;
    g_snapshot.updated_at_ms = g_now_ms;
    g_snapshot.source_age_ms = 0U;
    return true;
}
bool time_service_try_set_datetime(const DateTime *dt, TimeSourceType source) { return time_service_set_epoch_from_source(time_service_datetime_to_epoch(dt), source); }
void time_service_set_datetime(const DateTime *dt) { (void)time_service_try_set_datetime(dt, TIME_SOURCE_HOST_SYNC); }
void time_service_note_recovery_epoch(uint32_t epoch, bool valid) { (void)epoch; (void)valid; }
bool time_service_get_source_snapshot(TimeSourceSnapshot *out) { if (!out) return false; *out = g_snapshot; return true; }
const char *time_service_source_name(TimeSourceType source) { (void)source; return "TEST"; }
const char *time_service_confidence_name(TimeConfidenceLevel confidence) { (void)confidence; return "HIGH"; }
TimeAuthorityLevel time_service_authority_level(void) { return g_snapshot.authority; }
const char *time_service_authority_name(TimeAuthorityLevel authority) { (void)authority; return "NETWORK"; }
uint16_t time_service_day_id_from_epoch(uint32_t epoch) { return (uint16_t)(epoch / 86400U); }
bool time_service_is_reasonable_epoch(uint32_t epoch) { return epoch > 0U; }

ResetReason reset_reason_get(void) { return RESET_REASON_POWER_ON; }
const char *reset_reason_name(ResetReason reason) { (void)reason; return "POWER_ON"; }

void storage_facade_load_settings(SettingsState *settings) { (void)settings; }
void storage_facade_load_alarms(AlarmState *alarms, uint8_t count) { (void)alarms; (void)count; }
void storage_facade_load_game_stats(GameStatsState *stats) { (void)stats; }
void storage_facade_save_settings(const SettingsState *settings) { (void)settings; }
void storage_facade_save_alarms(const AlarmState *alarms, uint8_t count) { (void)alarms; (void)count; }
void storage_facade_save_game_stats(const GameStatsState *stats) { (void)stats; }
void storage_service_commit_now(void) {}
void storage_service_commit_now_with_reason(StorageCommitReason reason) { (void)reason; }
bool storage_service_is_initialized(void) { return true; }
uint8_t storage_service_get_version(void) { return 1U; }
uint8_t storage_service_get_schema_version(void) { return 1U; }
StorageBackendType storage_service_get_backend(void) { return STORAGE_BACKEND_FLASH; }
const char *storage_service_get_backend_name(void) { return "FLASH"; }
bool storage_service_get_backend_capabilities(StorageBackendCapabilities *out) { if (out) memset(out, 0, sizeof(*out)); return true; }
const char *storage_service_backend_phase_name(void) { return "READY"; }
const char *storage_service_backend_atomicity_name(StorageBackendAtomicityLevel level) { (void)level; return "ATOMIC"; }
const char *storage_service_backend_verify_mode_name(StorageBackendVerifyMode mode) { (void)mode; return "READBACK"; }
const char *storage_service_backend_latency_name(StorageBackendLatencyClass latency) { (void)latency; return "SYNC"; }
uint16_t storage_service_get_stored_crc(void) { return 0U; }
uint16_t storage_service_get_calculated_crc(void) { return 0U; }
bool storage_service_is_crc_valid(void) { return true; }
bool storage_service_is_flash_backend_ready(void) { return true; }
bool storage_service_app_state_durable_ready(void) { return true; }
uint8_t storage_service_get_pending_mask(void) { return 0U; }
uint32_t storage_service_get_commit_count(void) { return 0U; }
uint32_t storage_service_get_last_commit_ms(void) { return 0U; }
uint8_t storage_service_get_dirty_source_mask(void) { return 0U; }
bool storage_service_get_last_commit_ok(void) { return true; }
uint32_t storage_service_get_wear_count(void) { return 0U; }
void storage_service_get_wear_stats(StorageWearStats *out) { if (out) memset(out, 0, sizeof(*out)); }
StorageCommitReason storage_service_get_last_commit_reason(void) { return STORAGE_COMMIT_REASON_NONE; }
StorageCommitState storage_service_get_commit_state(void) { return STORAGE_COMMIT_STATE_IDLE; }

bool sensor_service_reinit(void) { return true; }
void sensor_request_calibration(void) {}
void sensor_request_reinit(void) {}
void diag_service_note_key_overflow(uint16_t overflow_count) { (void)overflow_count; }
void diag_service_note_time_invalid(void) {}
bool diag_service_is_safe_mode_active(void) { return false; }
DiagSafeModeReason diag_service_get_safe_mode_reason(void) { return DIAG_SAFE_MODE_NONE; }
const char *diag_service_safe_mode_reason_name(DiagSafeModeReason reason) { (void)reason; return "NONE"; }
void diag_service_clear_safe_mode_request(void) {}
WakeReason power_service_get_last_wake_reason(void) { return WAKE_REASON_BOOT; }
SleepReason power_service_get_last_sleep_reason(void) { return SLEEP_REASON_NONE; }
uint32_t power_service_get_last_wake_ms(void) { return 0U; }
uint32_t power_service_get_last_sleep_ms(void) { return 0U; }
uint16_t input_service_get_overflow_count(void) { return 0U; }

int main(void)
{
    DateTime dt = {.year = 2026U, .month = 4U, .day = 1U, .weekday = 3U, .hour = 7U, .minute = 55U, .second = 0U};
    g_snapshot.source = TIME_SOURCE_NETWORK_SYNC;
    g_snapshot.authority = TIME_AUTHORITY_NETWORK;
    g_snapshot.confidence = TIME_CONFIDENCE_HIGH;
    g_snapshot.valid = true;
    g_epoch = time_service_datetime_to_epoch(&dt);
    g_snapshot.epoch = g_epoch;

    model_init();
    model_set_alarm_enabled_at(0U, true);
    model_set_alarm_time_at(0U, 8U, 0U);
    model_set_alarm_enabled_at(1U, true);
    model_set_alarm_time_at(1U, 9U, 0U);

    assert(model_find_next_enabled_alarm() == 0U);
    model_trigger_alarm(0U, time_service_day_id_from_epoch(g_epoch));
    assert(model_get()->popup == POPUP_ALARM);
    model_snooze_alarm();
    assert(model_get()->popup == POPUP_NONE);
    assert(model_find_next_enabled_alarm() == 0U);

    g_epoch += 11U * 60U;
    g_snapshot.epoch = g_epoch;
    assert(model_find_next_enabled_alarm() == 1U);

    puts("[OK] model alarm + time integration check passed");
    return 0;
}

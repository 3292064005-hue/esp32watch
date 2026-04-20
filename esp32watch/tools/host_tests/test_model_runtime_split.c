#include "app_config.h"
#include "model.h"
#include "model_internal.h"
#include "services/input_service.h"
#include "services/power_service.h"
#include "services/storage_service.h"
#include "services/storage_facade.h"
#include "services/time_service.h"
#include "services/diag_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool storage_service_is_initialized(void) { return true; }
bool storage_service_is_crc_valid(void) { return true; }
uint16_t storage_service_get_stored_crc(void) { return 0x1234U; }
uint16_t storage_service_get_calculated_crc(void) { return 0x1234U; }
StorageBackendType storage_service_get_backend(void) { return STORAGE_BACKEND_FLASH; }
uint8_t storage_service_get_version(void) { return 6U; }
uint8_t storage_service_get_pending_mask(void) { return 0U; }
uint32_t storage_service_get_commit_count(void) { return 7U; }
uint32_t storage_service_get_last_commit_ms(void) { return 55U; }
uint8_t storage_service_get_dirty_source_mask(void) { return 0U; }
bool storage_service_get_last_commit_ok(void) { return true; }
StorageCommitReason storage_service_get_last_commit_reason(void) { return STORAGE_COMMIT_REASON_IDLE; }
void storage_service_commit_now(void) {}
WakeReason power_service_get_last_wake_reason(void) { return WAKE_REASON_KEY; }
SleepReason power_service_get_last_sleep_reason(void) { return SLEEP_REASON_AUTO_TIMEOUT; }
uint32_t power_service_get_last_sleep_ms(void) { return 11U; }
uint32_t power_service_get_last_wake_ms(void) { return 22U; }
uint16_t input_service_get_overflow_count(void) { return 0U; }
void diag_service_note_key_overflow(uint16_t overflow_count) { (void)overflow_count; }
void diag_service_note_time_invalid(void) {}

void storage_facade_load_settings(SettingsState *settings) { if (settings) memset(settings, 0, sizeof(*settings)); }
void storage_facade_save_settings(const SettingsState *settings) { (void)settings; }
void storage_facade_load_alarms(AlarmState *alarms, uint8_t count) { if (alarms) memset(alarms, 0, sizeof(*alarms) * count); }
void storage_facade_save_alarms(const AlarmState *alarms, uint8_t count) { (void)alarms; (void)count; }
void storage_facade_load_game_stats(GameStatsState *stats) { if (stats) memset(stats, 0, sizeof(*stats)); }
void storage_facade_save_game_stats(const GameStatsState *stats) { (void)stats; }

uint32_t time_service_get_epoch(void) { return 1704067200U; }
bool time_service_is_reasonable_epoch(uint32_t epoch) { return epoch != 0U; }
void time_service_note_recovery_epoch(uint32_t epoch, bool valid) { (void)epoch; (void)valid; }
bool time_service_get_datetime_snapshot(DateTime *out, TimeSourceSnapshot *source_out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->year = 2024U;
        out->month = 1U;
        out->day = 1U;
    }
    if (source_out != NULL) {
        memset(source_out, 0, sizeof(*source_out));
        source_out->valid = true;
        source_out->epoch = 1704067200U;
        source_out->source = TIME_SOURCE_DEVICE_CLOCK;
    }
    return (out != NULL) || (source_out != NULL);
}
uint16_t time_service_day_id_from_epoch(uint32_t epoch) { return (uint16_t)(epoch / 86400U); }
ResetReason reset_reason_get(void) { return RESET_REASON_POWER_ON; }
const char *reset_reason_name(ResetReason reason) { (void)reason; return "PWR"; }
uint32_t platform_time_now_ms(void) { return 1234U; }

int main(void)
{
    const WatchModel *compat = NULL;
    ModelDomainState domain_state;

    model_init();
    assert(model_get_domain_state(&domain_state) != NULL);
    assert(domain_state.alarm_selected == 0U);

    compat = model_get();
    assert(compat != NULL);
    assert(compat->alarm_selected == 0U);
    assert(compat->popup == POPUP_NONE);

    model_select_alarm(1U);
    model_set_alarm_time(7U, 30U);
    compat = model_get();
    assert(compat != NULL);
    assert(compat->alarm_selected == 1U);
    assert(compat->alarm.hour == 7U);
    assert(compat->alarm.minute == 30U);

    model_set_alarm_enabled(true);
    model_set_alarm_repeat_mask_at(1U, 0x7FU);
    assert(model_find_next_enabled_alarm() == 1U);

    model_push_popup(POPUP_LOW_BATTERY, false);
    compat = model_get();
    assert(compat != NULL);
    assert(compat->popup == POPUP_LOW_BATTERY);
    assert(compat->popup_latched);
    assert(compat->popup_queue_count == 1U);

    model_set_battery(4010U, 88U, true, true);
    compat = model_get();
    assert(compat != NULL);
    assert(compat->battery_mv == 4010U);
    assert(compat->battery_percent == 88U);
    assert(compat->charging);
    assert(compat->battery_present);

    model_note_manual_wake(1234U);
    compat = model_get();
    assert(compat != NULL);
    assert(compat->last_wake_ms == 1234U);
    assert(compat->last_user_activity_ms == 1234U);

    model_set_goal(12345U);
    model_set_screen_timeout_idx(2U);
    assert(model_get_domain_state(&domain_state) != NULL);
    assert(domain_state.settings.goal == 12345U);
    assert(domain_state.settings.screen_timeout_idx == 2U);
    compat = model_get();
    assert(compat != NULL);
    assert(compat->activity.goal == 12345U);
    assert(compat->settings.goal == 12345U);
    assert(compat->settings.screen_timeout_idx == 2U);
    assert(compat->screen_timeout_ms == SCREEN_SLEEP_MAX_MS);

    model_flush_read_snapshots();
    compat = model_get();
    assert(compat != NULL);
    assert(compat->alarm_selected == 1U);
    assert(compat->alarm.hour == 7U);
    assert(compat->alarm.minute == 30U);
    assert(compat->popup == POPUP_LOW_BATTERY);
    assert(compat->battery_mv == 4010U);
    assert(compat->last_wake_ms == 1234U);

    puts("[OK] model runtime public API + model_get compatibility snapshot check passed");
    return 0;
}

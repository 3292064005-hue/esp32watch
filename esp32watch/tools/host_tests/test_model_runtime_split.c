#include "model_internal.h"
#include "services/input_service.h"
#include "services/power_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

WatchModel g_model;

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
WakeReason power_service_get_last_wake_reason(void) { return WAKE_REASON_KEY; }
SleepReason power_service_get_last_sleep_reason(void) { return SLEEP_REASON_AUTO_TIMEOUT; }
uint32_t power_service_get_last_sleep_ms(void) { return 11U; }
uint32_t power_service_get_last_wake_ms(void) { return 22U; }
uint16_t input_service_get_overflow_count(void) { return 0U; }
void diag_service_note_key_overflow(uint16_t overflow_count) { (void)overflow_count; }

int main(void)
{
    memset(&g_model, 0, sizeof(g_model));
    memset(&g_model_runtime_state, 0, sizeof(g_model_runtime_state));
    memset(&g_model_domain_state, 0, sizeof(g_model_domain_state));
    memset(&g_model_ui_state, 0, sizeof(g_model_ui_state));

    g_model_domain_state.time_valid = true;
    g_model_domain_state.time_state = TIME_STATE_VALID;
    g_model_domain_state.alarm_selected = 1U;
    g_model_domain_state.alarms[1].hour = 7U;
    g_model_domain_state.alarms[1].minute = 30U;
    model_internal_commit_domain_mutation();
    assert(g_model.alarm_selected == 0U);
    assert(g_model.alarm.hour == 0U);
    assert(g_model.alarm.minute == 0U);

    g_model_ui_state.popup_queue[0] = POPUP_LOW_BATTERY;
    g_model_ui_state.popup_queue_count = 1U;
    model_internal_commit_ui_mutation();
    assert(g_model.popup == POPUP_NONE);

    model_runtime_apply_battery_snapshot(4010U, 88U, true, true);
    assert(g_model_runtime_state.battery_mv == 4010U);
    assert(g_model_runtime_state.battery_percent == 88U);
    assert(g_model_runtime_state.charging);
    assert(g_model_runtime_state.battery_present);
    assert(g_model.battery_mv == 0U);

    g_model.battery_mv = 3900U;
    g_model.battery_percent = 64U;
    g_model.charging = false;
    g_model.battery_present = true;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
    assert(g_model_runtime_state.battery_mv == 4010U);
    assert(g_model_runtime_state.battery_percent == 88U);
    assert(g_model_runtime_state.charging);
    assert(g_model_runtime_state.battery_present);
    assert(g_model.battery_mv == 3900U);

    model_runtime_note_manual_wake(1234U);
    assert(g_model_runtime_state.last_wake_ms == 1234U);
    assert(g_model_runtime_state.last_user_activity_ms == 1234U);
    assert(g_model.last_wake_ms == 0U);
    assert(g_model.last_user_activity_ms == 0U);

    puts("[OK] model_runtime split-source behavior check passed");
    return 0;
}

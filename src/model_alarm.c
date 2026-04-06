#include "model_internal.h"
#include "app_tuning.h"
#include "app_config.h"
#include "services/time_service.h"

static AlarmState *selected_alarm_slot(void)
{
    return &g_model.alarms[g_model.alarm_selected % APP_MAX_ALARMS];
}

void model_internal_sync_selected_alarm_view(void)
{
    g_model.alarm = *selected_alarm_slot();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

uint8_t model_get_alarm_count(void)
{
    return APP_MAX_ALARMS;
}

const AlarmState *model_get_alarm(uint8_t index)
{
    return &g_model.alarms[index % APP_MAX_ALARMS];
}

uint8_t model_get_selected_alarm_index(void)
{
    return g_model.alarm_selected;
}

void model_select_alarm(uint8_t index)
{
    g_model.alarm_selected = index % APP_MAX_ALARMS;
    model_internal_sync_selected_alarm_view();
}

void model_select_alarm_offset(int8_t delta)
{
    int8_t next = (int8_t)g_model.alarm_selected + delta;
    if (next < 0) next = (int8_t)APP_MAX_ALARMS - 1;
    if (next >= (int8_t)APP_MAX_ALARMS) next = 0;
    model_select_alarm((uint8_t)next);
}

uint8_t model_find_next_enabled_alarm(void)
{
    uint32_t now_epoch = time_service_get_epoch();
    int32_t best_delta = 0x7FFFFFFF;
    uint8_t best = 0xFFU;
    uint8_t now_wd = g_model.now.weekday % 7U;
    int32_t now_sec = (int32_t)(now_epoch % 86400UL);

    for (uint8_t i = 0; i < APP_MAX_ALARMS; ++i) {
        const AlarmState *a = &g_model.alarms[i];
        if (!a->enabled) {
            continue;
        }

        if (a->snooze_until_epoch > now_epoch) {
            int32_t snooze_delta = (int32_t)(a->snooze_until_epoch - now_epoch);
            if (snooze_delta < best_delta) {
                best_delta = snooze_delta;
                best = i;
            }
            continue;
        }

        for (uint8_t day_off = 0U; day_off < 7U; ++day_off) {
            int32_t target = (int32_t)(a->hour * 3600UL + a->minute * 60UL);
            int32_t delta = target - now_sec + (int32_t)day_off * 86400L;
            uint8_t weekday = (uint8_t)((now_wd + day_off) % 7U);
            bool weekday_ok = (a->repeat_mask == 0U) || ((a->repeat_mask & (1U << weekday)) != 0U);
            if (!weekday_ok) {
                continue;
            }
            if (day_off == 0U && delta < 0) {
                continue;
            }
            if (delta < best_delta) {
                best_delta = delta;
                best = i;
            }
            break;
        }
    }
    return best;
}

void model_trigger_alarm(uint8_t index, uint16_t due_day_id)
{
    AlarmState *a;

    if (index >= APP_MAX_ALARMS) {
        return;
    }

    a = &g_model.alarms[index];
    a->ringing = true;
    a->last_trigger_day = due_day_id;
    a->snooze_until_epoch = 0U;
    g_model.alarm_ringing_index = index;
    g_model.alarm_selected = index;
    g_model.alarm = *a;
    model_push_popup(POPUP_ALARM, true);
    model_internal_persist_all_alarms();
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STORAGE_COMMIT, STORAGE_COMMIT_REASON_ALARM);
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_UI);
}

void model_ack_popup(void)
{
    if (g_model.popup == POPUP_ALARM && g_model.alarm_ringing_index < APP_MAX_ALARMS) {
        AlarmState *a = &g_model.alarms[g_model.alarm_ringing_index];
        a->ringing = false;
        a->snooze_until_epoch = 0U;
        if (a->repeat_mask == 0U) {
            a->enabled = false;
        }
        model_internal_persist_all_alarms();
        model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STORAGE_COMMIT, STORAGE_COMMIT_REASON_ALARM);
        g_model.alarm_ringing_index = 0xFFU;
    } else if (g_model.popup == POPUP_TIMER_DONE && g_model.timer.remain_s == 0U) {
        g_model.timer.remain_s = g_model.timer.preset_s;
    }
    model_internal_pop_popup();
    if (g_model.sensor.online) {
        g_model.sensor_fault_latched = false;
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_UI);
}

void model_snooze_alarm(void)
{
    uint32_t epoch = time_service_get_epoch();
    if (g_model.alarm_ringing_index < APP_MAX_ALARMS) {
        AlarmState *a = &g_model.alarms[g_model.alarm_ringing_index];
        a->ringing = false;
        a->snooze_until_epoch = epoch + app_tuning_manifest_get()->alarm_snooze_minutes * 60UL;
        model_internal_persist_all_alarms();
        model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STORAGE_COMMIT, STORAGE_COMMIT_REASON_ALARM);
        g_model.alarm_ringing_index = 0xFFU;
    }
    model_internal_pop_popup();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_UI);
}

void model_set_alarm_enabled_at(uint8_t index, bool enabled)
{
    AlarmState *a = &g_model.alarms[index % APP_MAX_ALARMS];
    a->enabled = enabled;
    a->ringing = false;
    a->snooze_until_epoch = 0U;
    model_internal_persist_all_alarms();
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STORAGE_COMMIT, STORAGE_COMMIT_REASON_ALARM);
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_set_alarm_enabled(bool enabled)
{
    model_set_alarm_enabled_at(g_model.alarm_selected, enabled);
}

void model_set_alarm_time_at(uint8_t index, uint8_t hour, uint8_t minute)
{
    AlarmState *a = &g_model.alarms[index % APP_MAX_ALARMS];
    a->hour = hour % 24U;
    a->minute = minute % 60U;
    a->last_trigger_day = 0U;
    model_internal_persist_all_alarms();
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STORAGE_COMMIT, STORAGE_COMMIT_REASON_ALARM);
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_set_alarm_time(uint8_t hour, uint8_t minute)
{
    model_set_alarm_time_at(g_model.alarm_selected, hour, minute);
}

void model_set_alarm_repeat_mask_at(uint8_t index, uint8_t repeat_mask)
{
    AlarmState *a = &g_model.alarms[index % APP_MAX_ALARMS];
    a->repeat_mask = repeat_mask;
    a->last_trigger_day = 0U;
    model_internal_persist_all_alarms();
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STORAGE_COMMIT, STORAGE_COMMIT_REASON_ALARM);
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

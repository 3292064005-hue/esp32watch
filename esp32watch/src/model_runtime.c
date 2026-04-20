#include "model_internal.h"
#include "services/input_service.h"
#include "services/power_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include <string.h>

static uint16_t g_last_runtime_input_overflow_count;

ModelDomainState g_model_domain_state;
ModelRuntimeState g_model_runtime_state;
ModelUiState g_model_ui_state;

__attribute__((weak)) bool model_has_runtime_requests(void)
{
    return false;
}

static void model_internal_normalize_domain_state(void)
{
    uint8_t selected_index;

    if (APP_MAX_ALARMS == 0U) {
        g_model_domain_state.alarm_selected = 0U;
        memset(&g_model_domain_state.selected_alarm, 0, sizeof(g_model_domain_state.selected_alarm));
        return;
    }

    selected_index = (uint8_t)(g_model_domain_state.alarm_selected % APP_MAX_ALARMS);
    g_model_domain_state.alarm_selected = selected_index;
    g_model_domain_state.selected_alarm = g_model_domain_state.alarms[selected_index];
}

static void model_internal_normalize_ui_state(void)
{
    if (g_model_ui_state.popup_queue_count > APP_POPUP_QUEUE_DEPTH) {
        g_model_ui_state.popup_queue_count = APP_POPUP_QUEUE_DEPTH;
    }

    if (g_model_ui_state.popup_queue_count > 0U) {
        g_model_ui_state.popup = g_model_ui_state.popup_queue[0];
        g_model_ui_state.popup_latched = true;
    } else {
        g_model_ui_state.popup = POPUP_NONE;
        g_model_ui_state.popup_latched = false;
    }

    g_model_ui_state.has_runtime_requests = model_has_runtime_requests();
}

static const WatchModel *model_internal_populate_compat_snapshot(WatchModel *out)
{
    if (out == NULL) {
        return NULL;
    }

    model_internal_normalize_domain_state();
    model_internal_normalize_ui_state();
    memset(out, 0, sizeof(*out));

    out->now = g_model_domain_state.now;
    out->time_valid = g_model_domain_state.time_valid;
    out->time_state = g_model_domain_state.time_state;
    memcpy(out->alarms, g_model_domain_state.alarms, sizeof(out->alarms));
    out->alarm = g_model_domain_state.selected_alarm;
    out->alarm_selected = g_model_domain_state.alarm_selected;
    out->alarm_ringing_index = g_model_domain_state.alarm_ringing_index;
    out->stopwatch = g_model_domain_state.stopwatch;
    out->timer = g_model_domain_state.timer;
    out->activity = g_model_domain_state.activity;
    out->settings = g_model_domain_state.settings;
    out->game_stats = g_model_domain_state.game_stats;
    out->current_day_id = g_model_domain_state.current_day_id;

    out->sensor = g_model_runtime_state.sensor;
    out->battery_mv = g_model_runtime_state.battery_mv;
    out->battery_percent = g_model_runtime_state.battery_percent;
    out->battery_present = g_model_runtime_state.battery_present;
    out->charging = g_model_runtime_state.charging;
    out->storage_ok = g_model_runtime_state.storage_ok;
    out->storage_crc_ok = g_model_runtime_state.storage_crc_ok;
    out->storage_version = g_model_runtime_state.storage_version;
    out->storage_backend = g_model_runtime_state.storage_backend;
    out->storage_stored_crc = g_model_runtime_state.storage_stored_crc;
    out->storage_calc_crc = g_model_runtime_state.storage_calc_crc;
    out->storage_last_commit_ms = g_model_runtime_state.storage_last_commit_ms;
    out->storage_commit_count = g_model_runtime_state.storage_commit_count;
    out->storage_pending_mask = g_model_runtime_state.storage_pending_mask;
    out->input_queue_overflow_count = g_model_runtime_state.input_queue_overflow_count;
    out->reset_reason = g_model_runtime_state.reset_reason;
    out->last_wake_reason = g_model_runtime_state.last_wake_reason;
    out->last_sleep_reason = g_model_runtime_state.last_sleep_reason;
    out->last_sleep_ms = g_model_runtime_state.last_sleep_ms;
    out->storage_last_commit_ok = g_model_runtime_state.storage_last_commit_ok;
    out->storage_last_commit_reason = g_model_runtime_state.storage_last_commit_reason;
    out->storage_dirty_source_mask = g_model_runtime_state.storage_dirty_source_mask;
    out->last_user_activity_ms = g_model_runtime_state.last_user_activity_ms;
    out->last_wake_ms = g_model_runtime_state.last_wake_ms;
    out->last_render_ms = g_model_runtime_state.last_render_ms;
    out->screen_timeout_ms = g_model_runtime_state.screen_timeout_ms;

    out->popup = g_model_ui_state.popup;
    out->popup_latched = g_model_ui_state.popup_latched;
    memcpy(out->popup_queue, g_model_ui_state.popup_queue, sizeof(out->popup_queue));
    out->popup_queue_count = g_model_ui_state.popup_queue_count;
    out->sensor_fault_latched = g_model_ui_state.sensor_fault_latched;
    return out;
}


void model_internal_flush_read_models(void)
{
    model_internal_normalize_domain_state();
    model_internal_normalize_ui_state();
}

const WatchModel *model_internal_build_compat_snapshot(WatchModel *out)
{
    return model_internal_populate_compat_snapshot(out);
}


void model_internal_commit_runtime_mutation(void)
{
    model_internal_flush_read_models();
}

void model_internal_commit_domain_mutation(void)
{
    model_internal_flush_read_models();
}

void model_internal_commit_ui_mutation(void)
{
    model_internal_flush_read_models();
}


void model_flush_read_snapshots(void)
{
    model_internal_flush_read_models();
}

const ModelDomainState *model_get_domain_state(ModelDomainState *out)
{
    if (out == NULL) {
        return NULL;
    }

    model_internal_flush_read_models();
    *out = g_model_domain_state;
    return out;
}

const GameStatsState *model_get_game_stats(GameStatsState *out)
{
    if (out == NULL) {
        return NULL;
    }

    model_internal_flush_read_models();
    *out = g_model_domain_state.game_stats;
    return out;
}

const ModelRuntimeState *model_get_runtime_state(ModelRuntimeState *out)
{
    if (out == NULL) {
        return NULL;
    }

    model_internal_flush_read_models();
    *out = g_model_runtime_state;
    return out;
}

const ModelUiState *model_get_ui_state(ModelUiState *out)
{
    if (out == NULL) {
        return NULL;
    }

    model_internal_flush_read_models();
    *out = g_model_ui_state;
    return out;
}

void model_runtime_apply_battery_snapshot(uint16_t mv, uint8_t percent, bool charging, bool present)
{
    g_model_runtime_state.battery_mv = mv;
    g_model_runtime_state.battery_percent = percent > 100U ? 100U : percent;
    g_model_runtime_state.charging = charging;
    g_model_runtime_state.battery_present = present;
    model_internal_commit_runtime_mutation();
}

void model_runtime_apply_sensor_snapshot(const SensorSnapshot *snap)
{
    if (snap == NULL) {
        return;
    }

    g_model_runtime_state.sensor.online = snap->online;
    g_model_runtime_state.sensor.calibrated = snap->calibrated;
    g_model_runtime_state.sensor.calibration_pending = snap->calibration_pending;
    g_model_runtime_state.sensor.runtime_state = (uint8_t)snap->runtime_state;
    g_model_runtime_state.sensor.calibration_state = (uint8_t)snap->calibration_state;
    g_model_runtime_state.sensor.static_now = snap->static_now;
    g_model_runtime_state.sensor.ax = snap->ax;
    g_model_runtime_state.sensor.ay = snap->ay;
    g_model_runtime_state.sensor.az = snap->az;
    g_model_runtime_state.sensor.gx = snap->gx;
    g_model_runtime_state.sensor.gy = snap->gy;
    g_model_runtime_state.sensor.gz = snap->gz;
    g_model_runtime_state.sensor.accel_norm_mg = snap->accel_norm_mg;
    g_model_runtime_state.sensor.baseline_mg = snap->baseline_mg;
    g_model_runtime_state.sensor.motion_score = snap->motion_score;
    g_model_runtime_state.sensor.pitch_deg = snap->pitch_deg;
    g_model_runtime_state.sensor.roll_deg = snap->roll_deg;
    g_model_runtime_state.sensor.quality = snap->quality;
    g_model_runtime_state.sensor.calibration_progress = snap->calibration_progress;
    g_model_runtime_state.sensor.fault_count = snap->fault_count;
    g_model_runtime_state.sensor.reinit_count = snap->reinit_count;
    g_model_runtime_state.sensor.error_code = snap->error_code;
    g_model_runtime_state.sensor.last_sample_ms = snap->last_sample_ms;
    g_model_runtime_state.sensor.offline_since_ms = snap->offline_since_ms;
    g_model_runtime_state.sensor.steps_total = snap->steps_total;
    g_model_runtime_state.sensor.retry_backoff_ms = snap->retry_backoff_ms;
    model_internal_commit_runtime_mutation();
}

void model_runtime_apply_storage_observability(void)
{
    g_model_runtime_state.storage_ok = storage_service_is_initialized();
    g_model_runtime_state.storage_crc_ok = storage_service_is_crc_valid();
    g_model_runtime_state.storage_stored_crc = storage_service_get_stored_crc();
    g_model_runtime_state.storage_calc_crc = storage_service_get_calculated_crc();
    g_model_runtime_state.storage_backend = storage_service_get_backend();
    g_model_runtime_state.storage_version = storage_service_get_version();
    g_model_runtime_state.storage_pending_mask = storage_service_get_pending_mask();
    g_model_runtime_state.storage_commit_count = storage_service_get_commit_count();
    g_model_runtime_state.storage_last_commit_ms = storage_service_get_last_commit_ms();
    g_model_runtime_state.storage_dirty_source_mask = storage_service_get_dirty_source_mask();
    g_model_runtime_state.storage_last_commit_ok = storage_service_get_last_commit_ok() ? 1U : 0U;
    g_model_runtime_state.storage_last_commit_reason = storage_service_get_last_commit_reason();
    model_internal_commit_runtime_mutation();
}

void model_runtime_apply_power_observability(void)
{
    g_model_runtime_state.last_wake_reason = power_service_get_last_wake_reason();
    g_model_runtime_state.last_sleep_reason = power_service_get_last_sleep_reason();
    g_model_runtime_state.last_sleep_ms = power_service_get_last_sleep_ms();
    g_model_runtime_state.last_wake_ms = power_service_get_last_wake_ms();
    model_internal_commit_runtime_mutation();
}

void model_runtime_apply_input_observability(void)
{
    uint16_t overflow_count = input_service_get_overflow_count();

    g_model_runtime_state.input_queue_overflow_count = overflow_count;
    model_internal_commit_runtime_mutation();
    if (overflow_count != g_last_runtime_input_overflow_count) {
        g_last_runtime_input_overflow_count = overflow_count;
        diag_service_note_key_overflow(overflow_count);
    }
}

void model_runtime_note_user_activity(uint32_t now_ms)
{
    g_model_runtime_state.last_user_activity_ms = now_ms;
    model_internal_commit_runtime_mutation();
}

void model_runtime_note_render(uint32_t now_ms)
{
    g_model_runtime_state.last_render_ms = now_ms;
    model_internal_commit_runtime_mutation();
}

void model_runtime_note_manual_wake(uint32_t now_ms)
{
    g_model_runtime_state.last_wake_ms = now_ms;
    g_model_runtime_state.last_user_activity_ms = now_ms;
    model_internal_commit_runtime_mutation();
}

void model_internal_sync_storage_runtime(void)
{
    model_runtime_apply_storage_observability();
}

void model_internal_sync_power_runtime(void)
{
    model_runtime_apply_power_observability();
}

void model_internal_sync_input_runtime(void)
{
    model_runtime_apply_input_observability();
}

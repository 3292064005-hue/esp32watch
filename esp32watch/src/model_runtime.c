#include "model_internal.h"
#include "services/input_service.h"
#include "services/power_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include <string.h>

static uint16_t g_last_runtime_input_overflow_count;
static uint32_t g_model_projection_dirty_mask = MODEL_PROJECTION_DIRTY_ALL;

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

static void model_internal_rebuild_legacy_domain_projection(void)
{
    model_internal_normalize_domain_state();
    g_model.now = g_model_domain_state.now;
    g_model.time_valid = g_model_domain_state.time_valid;
    g_model.time_state = g_model_domain_state.time_state;
    memcpy(g_model.alarms, g_model_domain_state.alarms, sizeof(g_model.alarms));
    g_model.alarm = g_model_domain_state.selected_alarm;
    g_model.alarm_selected = g_model_domain_state.alarm_selected;
    g_model.alarm_ringing_index = g_model_domain_state.alarm_ringing_index;
    g_model.stopwatch = g_model_domain_state.stopwatch;
    g_model.timer = g_model_domain_state.timer;
    g_model.activity = g_model_domain_state.activity;
    g_model.settings = g_model_domain_state.settings;
    g_model.game_stats = g_model_domain_state.game_stats;
    g_model.current_day_id = g_model_domain_state.current_day_id;
}

static void model_internal_rebuild_legacy_runtime_projection(void)
{
    g_model.sensor = g_model_runtime_state.sensor;
    g_model.battery_mv = g_model_runtime_state.battery_mv;
    g_model.battery_percent = g_model_runtime_state.battery_percent;
    g_model.battery_present = g_model_runtime_state.battery_present;
    g_model.charging = g_model_runtime_state.charging;
    g_model.storage_ok = g_model_runtime_state.storage_ok;
    g_model.storage_crc_ok = g_model_runtime_state.storage_crc_ok;
    g_model.storage_version = g_model_runtime_state.storage_version;
    g_model.storage_backend = g_model_runtime_state.storage_backend;
    g_model.storage_stored_crc = g_model_runtime_state.storage_stored_crc;
    g_model.storage_calc_crc = g_model_runtime_state.storage_calc_crc;
    g_model.storage_last_commit_ms = g_model_runtime_state.storage_last_commit_ms;
    g_model.storage_commit_count = g_model_runtime_state.storage_commit_count;
    g_model.storage_pending_mask = g_model_runtime_state.storage_pending_mask;
    g_model.input_queue_overflow_count = g_model_runtime_state.input_queue_overflow_count;
    g_model.reset_reason = g_model_runtime_state.reset_reason;
    g_model.last_wake_reason = g_model_runtime_state.last_wake_reason;
    g_model.last_sleep_reason = g_model_runtime_state.last_sleep_reason;
    g_model.last_sleep_ms = g_model_runtime_state.last_sleep_ms;
    g_model.storage_last_commit_ok = g_model_runtime_state.storage_last_commit_ok;
    g_model.storage_last_commit_reason = g_model_runtime_state.storage_last_commit_reason;
    g_model.storage_dirty_source_mask = g_model_runtime_state.storage_dirty_source_mask;
    g_model.last_user_activity_ms = g_model_runtime_state.last_user_activity_ms;
    g_model.last_wake_ms = g_model_runtime_state.last_wake_ms;
    g_model.last_render_ms = g_model_runtime_state.last_render_ms;
    g_model.screen_timeout_ms = g_model_runtime_state.screen_timeout_ms;
}

static void model_internal_rebuild_legacy_ui_projection(void)
{
    model_internal_normalize_ui_state();
    g_model.popup = g_model_ui_state.popup;
    g_model.popup_latched = g_model_ui_state.popup_latched;
    memcpy(g_model.popup_queue, g_model_ui_state.popup_queue, sizeof(g_model.popup_queue));
    g_model.popup_queue_count = g_model_ui_state.popup_queue_count;
    g_model.sensor_fault_latched = g_model_ui_state.sensor_fault_latched;
}

void model_internal_mark_projection_dirty(uint32_t flags)
{
    if ((flags & MODEL_PROJECTION_DIRTY_ALL) == 0U) {
        return;
    }
    g_model_projection_dirty_mask |= (flags & MODEL_PROJECTION_DIRTY_ALL);
}

void model_internal_flush_read_models(void)
{
    const uint32_t dirty_mask = g_model_projection_dirty_mask;

    if (dirty_mask == MODEL_PROJECTION_DIRTY_NONE) {
        return;
    }

#if !MODEL_ENABLE_LEGACY_PROJECTION
    g_model_projection_dirty_mask = MODEL_PROJECTION_DIRTY_NONE;
    return;
#endif

    if ((dirty_mask & MODEL_PROJECTION_DIRTY_DOMAIN) != 0U) {
        model_internal_rebuild_legacy_domain_projection();
    }
    if ((dirty_mask & MODEL_PROJECTION_DIRTY_RUNTIME) != 0U) {
        model_internal_rebuild_legacy_runtime_projection();
    }
    if ((dirty_mask & MODEL_PROJECTION_DIRTY_UI) != 0U) {
        model_internal_rebuild_legacy_ui_projection();
    }
    g_model_projection_dirty_mask = MODEL_PROJECTION_DIRTY_NONE;
}

void model_internal_commit_legacy_mutation_with_flags(uint32_t flags)
{
    /*
     * The aggregate WatchModel view is a compatibility projection only.
     * Any attempted direct edits are discarded by rebuilding the requested
     * projection slices from the authoritative split-state domains.
     */
    model_internal_mark_projection_dirty((flags & MODEL_PROJECTION_DIRTY_ALL) != 0U ? flags : MODEL_PROJECTION_DIRTY_ALL);
    model_internal_flush_read_models();
}

void model_internal_commit_legacy_mutation(void)
{
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_ALL);
}

void model_internal_commit_runtime_mutation_with_flags(uint32_t flags)
{
    model_internal_mark_projection_dirty(flags | MODEL_PROJECTION_DIRTY_RUNTIME);
    model_internal_flush_read_models();
}

void model_internal_commit_runtime_mutation(void)
{
    model_internal_commit_runtime_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_internal_commit_domain_mutation_with_flags(uint32_t flags)
{
    model_internal_mark_projection_dirty(flags | MODEL_PROJECTION_DIRTY_DOMAIN);
    model_internal_flush_read_models();
}

void model_internal_commit_domain_mutation(void)
{
    model_internal_commit_domain_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_internal_commit_ui_mutation_with_flags(uint32_t flags)
{
    model_internal_mark_projection_dirty(flags | MODEL_PROJECTION_DIRTY_UI);
    model_internal_flush_read_models();
}

void model_internal_commit_ui_mutation(void)
{
    model_internal_commit_ui_mutation_with_flags(MODEL_PROJECTION_DIRTY_UI);
}

void model_internal_sync_split_state_from_legacy(void)
{
    /* Legacy writes are not authoritative anymore; rebuild the projection only. */
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_ALL);
}

void model_internal_sync_legacy_from_split_state(void)
{
    model_internal_mark_projection_dirty(MODEL_PROJECTION_DIRTY_ALL);
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

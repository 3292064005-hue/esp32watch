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

static void model_internal_reconcile_legacy_views(void)
{
    uint8_t selected_index = (uint8_t)(g_model.alarm_selected % APP_MAX_ALARMS);

    g_model.alarm_selected = selected_index;
    g_model.alarm = g_model.alarms[selected_index];

    if (g_model.popup_queue_count > APP_POPUP_QUEUE_DEPTH) {
        g_model.popup_queue_count = APP_POPUP_QUEUE_DEPTH;
    }
    if (g_model.popup_queue_count > 0U) {
        g_model.popup = g_model.popup_queue[0];
        g_model.popup_latched = true;
    } else {
        g_model.popup = POPUP_NONE;
        g_model.popup_latched = false;
    }
}

static void model_internal_rebuild_domain_state(void)
{
    memset(&g_model_domain_state, 0, sizeof(g_model_domain_state));
    g_model_domain_state.now = g_model.now;
    g_model_domain_state.time_valid = g_model.time_valid;
    g_model_domain_state.time_state = g_model.time_state;
    memcpy(g_model_domain_state.alarms, g_model.alarms, sizeof(g_model_domain_state.alarms));
    g_model_domain_state.selected_alarm = g_model.alarm;
    g_model_domain_state.alarm_selected = g_model.alarm_selected;
    g_model_domain_state.alarm_ringing_index = g_model.alarm_ringing_index;
    g_model_domain_state.stopwatch = g_model.stopwatch;
    g_model_domain_state.timer = g_model.timer;
    g_model_domain_state.activity = g_model.activity;
    g_model_domain_state.settings = g_model.settings;
    g_model_domain_state.current_day_id = g_model.current_day_id;
}

static void model_internal_rebuild_runtime_state(void)
{
    memset(&g_model_runtime_state, 0, sizeof(g_model_runtime_state));
    g_model_runtime_state.sensor = g_model.sensor;
    g_model_runtime_state.battery_mv = g_model.battery_mv;
    g_model_runtime_state.battery_percent = g_model.battery_percent;
    g_model_runtime_state.battery_present = g_model.battery_present;
    g_model_runtime_state.charging = g_model.charging;
    g_model_runtime_state.storage_ok = g_model.storage_ok;
    g_model_runtime_state.storage_crc_ok = g_model.storage_crc_ok;
    g_model_runtime_state.storage_version = g_model.storage_version;
    g_model_runtime_state.storage_backend = g_model.storage_backend;
    g_model_runtime_state.storage_stored_crc = g_model.storage_stored_crc;
    g_model_runtime_state.storage_calc_crc = g_model.storage_calc_crc;
    g_model_runtime_state.storage_last_commit_ms = g_model.storage_last_commit_ms;
    g_model_runtime_state.storage_commit_count = g_model.storage_commit_count;
    g_model_runtime_state.storage_pending_mask = g_model.storage_pending_mask;
    g_model_runtime_state.input_queue_overflow_count = g_model.input_queue_overflow_count;
    g_model_runtime_state.reset_reason = g_model.reset_reason;
    g_model_runtime_state.last_wake_reason = g_model.last_wake_reason;
    g_model_runtime_state.last_sleep_reason = g_model.last_sleep_reason;
    g_model_runtime_state.last_sleep_ms = g_model.last_sleep_ms;
    g_model_runtime_state.storage_last_commit_ok = g_model.storage_last_commit_ok;
    g_model_runtime_state.storage_last_commit_reason = g_model.storage_last_commit_reason;
    g_model_runtime_state.storage_dirty_source_mask = g_model.storage_dirty_source_mask;
    g_model_runtime_state.last_user_activity_ms = g_model.last_user_activity_ms;
    g_model_runtime_state.last_wake_ms = g_model.last_wake_ms;
    g_model_runtime_state.last_render_ms = g_model.last_render_ms;
    g_model_runtime_state.screen_timeout_ms = g_model.screen_timeout_ms;
}

static void model_internal_rebuild_ui_state(void)
{
    memset(&g_model_ui_state, 0, sizeof(g_model_ui_state));
    g_model_ui_state.popup = g_model.popup;
    g_model_ui_state.popup_latched = g_model.popup_latched;
    memcpy(g_model_ui_state.popup_queue, g_model.popup_queue, sizeof(g_model_ui_state.popup_queue));
    g_model_ui_state.popup_queue_count = g_model.popup_queue_count;
    g_model_ui_state.sensor_fault_latched = g_model.sensor_fault_latched;
    g_model_ui_state.has_runtime_requests = model_has_runtime_requests();
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
    uint32_t dirty_mask = g_model_projection_dirty_mask;

    if (dirty_mask == MODEL_PROJECTION_DIRTY_NONE) {
        return;
    }

    model_internal_reconcile_legacy_views();
    if ((dirty_mask & MODEL_PROJECTION_DIRTY_DOMAIN) != 0U) {
        model_internal_rebuild_domain_state();
    }
    if ((dirty_mask & MODEL_PROJECTION_DIRTY_RUNTIME) != 0U) {
        model_internal_rebuild_runtime_state();
    }
    if ((dirty_mask & MODEL_PROJECTION_DIRTY_UI) != 0U) {
        model_internal_rebuild_ui_state();
    }
    g_model_projection_dirty_mask = MODEL_PROJECTION_DIRTY_NONE;
}

void model_internal_commit_legacy_mutation_with_flags(uint32_t flags)
{
    model_internal_reconcile_legacy_views();
    model_internal_mark_projection_dirty(flags);
}

void model_internal_commit_legacy_mutation(void)
{
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_ALL);
}

void model_internal_sync_split_state_from_legacy(void)
{
    model_internal_mark_projection_dirty(MODEL_PROJECTION_DIRTY_ALL);
    model_internal_flush_read_models();
}

void model_internal_sync_legacy_from_split_state(void)
{
    /* Compatibility hook: g_model remains the single writable source of truth. */
    model_internal_reconcile_legacy_views();
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

void model_internal_sync_storage_runtime(void)
{
    g_model.storage_ok = storage_service_is_initialized();
    g_model.storage_crc_ok = storage_service_is_crc_valid();
    g_model.storage_stored_crc = storage_service_get_stored_crc();
    g_model.storage_calc_crc = storage_service_get_calculated_crc();
    g_model.storage_backend = storage_service_get_backend();
    g_model.storage_version = storage_service_get_version();
    g_model.storage_pending_mask = storage_service_get_pending_mask();
    g_model.storage_commit_count = storage_service_get_commit_count();
    g_model.storage_last_commit_ms = storage_service_get_last_commit_ms();
    g_model.storage_dirty_source_mask = storage_service_get_dirty_source_mask();
    g_model.storage_last_commit_ok = storage_service_get_last_commit_ok() ? 1U : 0U;
    g_model.storage_last_commit_reason = storage_service_get_last_commit_reason();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_internal_sync_power_runtime(void)
{
    g_model.last_wake_reason = power_service_get_last_wake_reason();
    g_model.last_sleep_reason = power_service_get_last_sleep_reason();
    g_model.last_sleep_ms = power_service_get_last_sleep_ms();
    g_model.last_wake_ms = power_service_get_last_wake_ms();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_internal_sync_input_runtime(void)
{
    uint16_t overflow_count = input_service_get_overflow_count();

    g_model.input_queue_overflow_count = overflow_count;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
    if (overflow_count != g_last_runtime_input_overflow_count) {
        g_last_runtime_input_overflow_count = overflow_count;
        diag_service_note_key_overflow(overflow_count);
    }
}

#include "watch_app_internal.h"
#include "app_config.h"
#include "model.h"
#include "services/activity_service.h"
#include "services/display_service.h"
#include "services/diag_service.h"
#include "services/time_service.h"
#include "services/alarm_service.h"
#include "services/alert_service.h"
#include "services/input_service.h"
#include "services/power_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/wdt_service.h"
#include "services/battery_service.h"
#include "crash_capsule.h"
#include "power_policy.h"
#include "ui.h"
#include "ui_internal.h"
#include "web/web_overlay.h"
#include "platform_api.h"
#include <string.h>

__attribute__((weak)) uint32_t watch_app_stage_clock_now(void)
{
    return platform_time_now_ms();
}

static const char * const g_stage_names[WATCH_APP_STAGE_COUNT] = {
    "INPUT",
    "SENSOR",
    "MODEL",
    "UI",
    "BATTERY",
    "ALERT",
    "DIAG",
    "STORAGE",
    "RENDER",
    "IDLE"
};

static bool watch_app_qos_provider_render(void *ctx, PowerQosSnapshot *snapshot)
{
    const WatchAppRuntimeState *state = (const WatchAppRuntimeState *)ctx;

    if (snapshot == NULL || state == NULL) {
        return false;
    }
    snapshot->render_due = snapshot->render_due || state->qos_render_due;
    return true;
}

static bool watch_app_qos_provider_storage(void *ctx, PowerQosSnapshot *snapshot)
{
    (void)ctx;
    if (snapshot == NULL) {
        return false;
    }
    snapshot->storage_pending = snapshot->storage_pending || storage_service_has_pending();
    snapshot->transaction_active = snapshot->transaction_active || storage_service_is_transaction_active();
    return true;
}

static bool watch_app_qos_provider_diag(void *ctx, PowerQosSnapshot *snapshot)
{
    (void)ctx;
    if (snapshot == NULL) {
        return false;
    }
    snapshot->safe_mode_active = snapshot->safe_mode_active || diag_service_is_safe_mode_active();
    return true;
}

static bool watch_app_qos_provider_sensor(void *ctx, PowerQosSnapshot *snapshot)
{
    const WatchAppRuntimeState *state = (const WatchAppRuntimeState *)ctx;

    if (snapshot == NULL || state == NULL) {
        return false;
    }
    snapshot->sensor_backoff_active = snapshot->sensor_backoff_active || state->qos_sensor_backoff_active;
    return true;
}

static bool watch_app_qos_provider_ui(void *ctx, PowerQosSnapshot *snapshot)
{
    const WatchAppRuntimeState *state = (const WatchAppRuntimeState *)ctx;

    if (snapshot == NULL || state == NULL) {
        return false;
    }
    snapshot->ui_feedback_pending = snapshot->ui_feedback_pending || state->qos_ui_feedback_pending;
    return true;
}

static bool watch_app_qos_provider_alarm(void *ctx, PowerQosSnapshot *snapshot)
{
    const WatchAppRuntimeState *state = (const WatchAppRuntimeState *)ctx;

    if (snapshot == NULL || state == NULL) {
        return false;
    }
    snapshot->alarm_active = snapshot->alarm_active || state->qos_alarm_active;
    return true;
}

void watch_app_register_qos_providers(WatchAppRuntimeState *state)
{
    power_policy_reset_qos_registry();
    (void)power_policy_register_qos_provider(watch_app_qos_provider_render, state);
    (void)power_policy_register_qos_provider(watch_app_qos_provider_storage, state);
    (void)power_policy_register_qos_provider(watch_app_qos_provider_diag, state);
    (void)power_policy_register_qos_provider(watch_app_qos_provider_sensor, state);
    (void)power_policy_register_qos_provider(watch_app_qos_provider_ui, state);
    (void)power_policy_register_qos_provider(watch_app_qos_provider_alarm, state);
}

static void watch_app_run_input_stage(WatchAppRuntimeState *state, uint32_t now)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    if ((now - state->last_key_scan) >= 10U) {
        state->last_key_scan = now;
        input_service_tick();
    }
    watch_app_record_stage_timing(WATCH_APP_STAGE_INPUT,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_INPUT, watch_app_result_input_stage(), platform_time_now_ms());
}

static void watch_app_run_sensor_stage(WatchAppRuntimeState *state, uint32_t now)
{
    const SensorSnapshot *snap;
    ModelDomainState domain_state;
    uint32_t stage_start = watch_app_stage_clock_now();

    if (model_get_domain_state(&domain_state) != NULL &&
        state->last_sensor_sensitivity != domain_state.settings.sensor_sensitivity) {
        state->last_sensor_sensitivity = domain_state.settings.sensor_sensitivity;
        sensor_service_set_sensitivity(state->last_sensor_sensitivity);
    }

    sensor_service_tick(now);
    snap = sensor_service_get_snapshot();
    activity_service_ingest_snapshot(snap, now);
    state->qos_sensor_backoff_active = (snap != NULL) && (snap->runtime_state == SENSOR_STATE_BACKOFF);
    watch_app_record_stage_timing(WATCH_APP_STAGE_SENSOR,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_SENSOR, watch_app_result_sensor_stage(snap), platform_time_now_ms());
}

static void watch_app_run_model_stage(WatchAppRuntimeState *state, uint32_t now)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    model_tick(now);
    alarm_service_tick(now);
    watch_app_apply_model_runtime_requests(&state->last_sensor_sensitivity);
    watch_app_record_stage_timing(WATCH_APP_STAGE_MODEL,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_MODEL, watch_app_result_model_stage(), platform_time_now_ms());
}

static void watch_app_run_ui_stage(WatchAppRuntimeState *state, uint32_t now)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    ui_tick(now);
    watch_app_apply_model_runtime_requests(&state->last_sensor_sensitivity);
    watch_app_apply_ui_actions(&state->sleep_request, &state->last_sensor_sensitivity);
    state->qos_ui_feedback_pending = ui_has_pending_actions();
    watch_app_record_stage_timing(WATCH_APP_STAGE_UI,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_UI, WDT_CHECKPOINT_RESULT_OK, platform_time_now_ms());
}

static void watch_app_run_simple_stage(WatchAppRuntimeState *state,
                                       WatchAppStageId stage,
                                       void (*fn)(uint32_t),
                                       uint32_t now,
                                       WdtCheckpoint checkpoint)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    fn(now);
    watch_app_record_stage_timing(stage,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(checkpoint, WDT_CHECKPOINT_RESULT_OK, platform_time_now_ms());
}

static void watch_app_run_diag_stage(WatchAppRuntimeState *state, uint32_t now)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    if (watch_app_should_run_diag_stage(state->stage_telemetry, state->loop_counter)) {
        diag_service_tick(now);
    } else {
        watch_app_note_stage_deferred(WATCH_APP_STAGE_DIAG,
                                      state->stage_telemetry,
                                      state->stage_history,
                                      &state->stage_history_head,
                                      &state->stage_history_count,
                                      state->loop_counter);
    }
    watch_app_record_stage_timing(WATCH_APP_STAGE_DIAG,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_DIAG, watch_app_result_diag_stage(), platform_time_now_ms());
}

static void watch_app_run_storage_stage(WatchAppRuntimeState *state, uint32_t now)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    if (watch_app_should_run_storage_stage(state->stage_telemetry, state->loop_counter)) {
        storage_service_tick(now);
    } else {
        watch_app_note_stage_deferred(WATCH_APP_STAGE_STORAGE,
                                      state->stage_telemetry,
                                      state->stage_history,
                                      &state->stage_history_head,
                                      &state->stage_history_count,
                                      state->loop_counter);
    }
    watch_app_after_storage_tick(&state->sleep_request, &state->last_storage_commit_ms);
    model_sync_runtime_observability();
    watch_app_record_stage_timing(WATCH_APP_STAGE_STORAGE,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_STORAGE, watch_app_result_storage_stage(), platform_time_now_ms());
}

static void watch_app_run_render_stage(WatchAppRuntimeState *state, uint32_t now)
{
    bool render_due;
    uint32_t stage_start = watch_app_stage_clock_now();

    render_due = ui_should_render(now);
    state->qos_render_due = render_due;
    if (render_due && !watch_app_should_defer_render(state->stage_telemetry, state->loop_counter)) {
        if (!web_overlay_render_if_active(now)) {
            ui_render();
        }
        state->qos_render_due = false;
        wdt_service_note_checkpoint_result(WDT_CHECKPOINT_RENDER, WDT_CHECKPOINT_RESULT_OK, platform_time_now_ms());
    } else if (render_due) {
        watch_app_note_stage_deferred(WATCH_APP_STAGE_RENDER,
                                      state->stage_telemetry,
                                      state->stage_history,
                                      &state->stage_history_head,
                                      &state->stage_history_count,
                                      state->loop_counter);
    }
    watch_app_record_stage_timing(WATCH_APP_STAGE_RENDER,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
}

static void watch_app_run_idle_stage(WatchAppRuntimeState *state)
{
    bool can_idle_sleep;
    ModelDomainState domain_state;
    PowerQosSnapshot qos_snapshot;
    uint32_t stage_start = watch_app_stage_clock_now();

    if (model_get_domain_state(&domain_state) != NULL) {
        state->qos_alarm_active = domain_state.alarm_ringing_index < APP_MAX_ALARMS;
    } else {
        state->qos_alarm_active = false;
    }
    power_policy_collect_qos_snapshot(&qos_snapshot);
    can_idle_sleep = power_policy_can_enter_cpu_idle_ex(&qos_snapshot, NULL);
    if (can_idle_sleep) {
        wdt_service_note_checkpoint_result(WDT_CHECKPOINT_IDLE, WDT_CHECKPOINT_RESULT_OK, platform_time_now_ms());
    }
    wdt_service_tick(platform_time_now_ms(), true);
    power_service_idle(can_idle_sleep);
    watch_app_record_stage_timing(WATCH_APP_STAGE_IDLE,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
}

void watch_app_runtime_run_loop(WatchAppRuntimeState *state)
{
    uint32_t now;

    if (state == NULL) {
        return;
    }

    now = platform_time_now_ms();
    state->loop_counter++;
    wdt_service_begin_loop(now);

    watch_app_run_input_stage(state, now);
    watch_app_run_sensor_stage(state, now);
    watch_app_run_model_stage(state, now);
    watch_app_run_ui_stage(state, now);
    watch_app_run_simple_stage(state, WATCH_APP_STAGE_BATTERY, battery_service_tick, now, WDT_CHECKPOINT_BATTERY);
    watch_app_run_simple_stage(state, WATCH_APP_STAGE_ALERT, alert_service_tick, now, WDT_CHECKPOINT_ALERT);
    watch_app_run_diag_stage(state, now);
    watch_app_run_storage_stage(state, now);
    watch_app_run_render_stage(state, now);
    watch_app_run_idle_stage(state);
    model_flush_read_snapshots();
}

const char *watch_app_runtime_stage_name(WatchAppStageId id)
{
    return (id < WATCH_APP_STAGE_COUNT) ? g_stage_names[id] : "UNKNOWN";
}

bool watch_app_runtime_get_stage_telemetry(const WatchAppRuntimeState *state,
                                           WatchAppStageId id,
                                           WatchAppStageTelemetry *out)
{
    if (state == NULL || id >= WATCH_APP_STAGE_COUNT || out == NULL) {
        return false;
    }
    *out = state->stage_telemetry[id];
    return true;
}

uint8_t watch_app_runtime_get_stage_history_count(const WatchAppRuntimeState *state)
{
    return state == NULL ? 0U : state->stage_history_count;
}

bool watch_app_runtime_get_stage_history_recent(const WatchAppRuntimeState *state,
                                                uint8_t reverse_index,
                                                WatchAppStageHistoryEntry *out)
{
    uint8_t index;

    if (state == NULL || out == NULL || reverse_index >= state->stage_history_count) {
        return false;
    }

    index = (uint8_t)((state->stage_history_head + WATCH_APP_STAGE_HISTORY_CAPACITY - 1U - reverse_index) % WATCH_APP_STAGE_HISTORY_CAPACITY);
    *out = state->stage_history[index];
    return true;
}


#include "watch_app_internal.h"
#include "app_config.h"
#include "app_tuning.h"
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
#include "services/network_sync_service.h"
#include "crash_capsule.h"
#include "power_policy.h"
#include "ui.h"
#include "ui_internal.h"
#include "web/web_overlay.h"
#include "web/web_server.h"
#include "web/web_wifi.h"
#include "web/web_action_queue.h"
#include "platform_api.h"
#include <string.h>

typedef void (*WatchAppStageRunFn)(WatchAppRuntimeState *state, uint32_t now_ms);

typedef struct {
    WatchAppStageId id;
    const char *name;
    WatchAppStageRunFn run_fn;
    uint32_t budget_ms;
    WdtCheckpoint wdt_checkpoint;
    uint32_t qos_mask;
    bool state_exposed;
    bool transaction_pm_lock;
} WatchAppStageDescriptor;

__attribute__((weak)) uint32_t watch_app_stage_clock_now(void)
{
    return platform_time_now_ms();
}

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

static bool watch_app_qos_provider_network(void *ctx, PowerQosSnapshot *snapshot)
{
    (void)ctx;
    if (snapshot == NULL) {
        return false;
    }
    snapshot->network_busy = snapshot->network_busy || web_wifi_transition_active();
    return true;
}

static bool watch_app_qos_provider_web(void *ctx, PowerQosSnapshot *snapshot)
{
    (void)ctx;
    if (snapshot == NULL) {
        return false;
    }
    snapshot->web_busy = snapshot->web_busy || web_wifi_access_point_active() || web_server_has_pending_actions();
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
    (void)power_policy_register_qos_provider(watch_app_qos_provider_network, state);
    (void)power_policy_register_qos_provider(watch_app_qos_provider_web, state);
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
    if (checkpoint != WDT_CHECKPOINT_NONE) {
        wdt_service_note_checkpoint_result(checkpoint, WDT_CHECKPOINT_RESULT_OK, platform_time_now_ms());
    }
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


static void watch_app_run_network_stage(WatchAppRuntimeState *state, uint32_t now)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    network_sync_service_tick(now);
    watch_app_record_stage_timing(WATCH_APP_STAGE_NETWORK,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_NETWORK, watch_app_result_network_stage(), platform_time_now_ms());
}

static void watch_app_run_web_stage(WatchAppRuntimeState *state, uint32_t now)
{
    uint32_t stage_start = watch_app_stage_clock_now();

    web_server_poll(now);
    watch_app_record_stage_timing(WATCH_APP_STAGE_WEB,
                                  stage_start,
                                  watch_app_stage_clock_now(),
                                  state->stage_telemetry,
                                  state->stage_history,
                                  &state->stage_history_head,
                                  &state->stage_history_count,
                                  state->loop_counter);
    wdt_service_note_checkpoint_result(WDT_CHECKPOINT_WEB, watch_app_result_web_stage(), platform_time_now_ms());
}

static void watch_app_run_battery_stage(WatchAppRuntimeState *state, uint32_t now)
{
    watch_app_run_simple_stage(state,
                               WATCH_APP_STAGE_BATTERY,
                               battery_service_tick,
                               now,
                               WDT_CHECKPOINT_BATTERY);
}

static void watch_app_run_alert_stage(WatchAppRuntimeState *state, uint32_t now)
{
    watch_app_run_simple_stage(state,
                               WATCH_APP_STAGE_ALERT,
                               alert_service_tick,
                               now,
                               WDT_CHECKPOINT_ALERT);
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

static void watch_app_run_idle_stage(WatchAppRuntimeState *state, uint32_t now)
{
    bool can_idle_sleep;
    ModelDomainState domain_state;
    PowerQosSnapshot qos_snapshot;
    uint32_t stage_start = watch_app_stage_clock_now();

    (void)now;

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

static const WatchAppStageDescriptor g_watch_app_stage_descriptors[] = {
    {WATCH_APP_STAGE_INPUT,   "INPUT",   watch_app_run_input_stage,   WATCH_APP_STAGE_INPUT_BUDGET_MS,   WDT_CHECKPOINT_INPUT,   POWER_QOS_NONE,                                         true,  false},
    {WATCH_APP_STAGE_SENSOR,  "SENSOR",  watch_app_run_sensor_stage,  WATCH_APP_STAGE_SENSOR_BUDGET_MS,  WDT_CHECKPOINT_SENSOR,  POWER_QOS_SENSOR_BACKOFF_ACTIVE,                       true,  true},
    {WATCH_APP_STAGE_MODEL,   "MODEL",   watch_app_run_model_stage,   WATCH_APP_STAGE_MODEL_BUDGET_MS,   WDT_CHECKPOINT_MODEL,   POWER_QOS_NONE,                                         true,  false},
    {WATCH_APP_STAGE_UI,      "UI",      watch_app_run_ui_stage,      WATCH_APP_STAGE_UI_BUDGET_MS,      WDT_CHECKPOINT_UI,      POWER_QOS_UI_FEEDBACK_PENDING,                         true,  false},
    {WATCH_APP_STAGE_BATTERY, "BATTERY", watch_app_run_battery_stage, WATCH_APP_STAGE_BATTERY_BUDGET_MS, WDT_CHECKPOINT_BATTERY, POWER_QOS_NONE,                                         true,  false},
    {WATCH_APP_STAGE_ALERT,   "ALERT",   watch_app_run_alert_stage,   WATCH_APP_STAGE_ALERT_BUDGET_MS,   WDT_CHECKPOINT_ALERT,   POWER_QOS_ALARM_ACTIVE,                                true,  false},
    {WATCH_APP_STAGE_DIAG,    "DIAG",    watch_app_run_diag_stage,    WATCH_APP_STAGE_DIAG_BUDGET_MS,    WDT_CHECKPOINT_DIAG,    POWER_QOS_SAFE_MODE_ACTIVE,                            true,  false},
    {WATCH_APP_STAGE_STORAGE, "STORAGE", watch_app_run_storage_stage, WATCH_APP_STAGE_STORAGE_BUDGET_MS, WDT_CHECKPOINT_STORAGE, POWER_QOS_STORAGE_PENDING | POWER_QOS_TRANSACTION_ACTIVE, true,  true},
    {WATCH_APP_STAGE_NETWORK, "NETWORK", watch_app_run_network_stage, WATCH_APP_STAGE_NETWORK_BUDGET_MS, WDT_CHECKPOINT_NETWORK, POWER_QOS_NETWORK_BUSY,                                 true,  true},
    {WATCH_APP_STAGE_WEB,     "WEB",     watch_app_run_web_stage,     WATCH_APP_STAGE_WEB_BUDGET_MS,     WDT_CHECKPOINT_WEB,     POWER_QOS_WEB_BUSY,                                     true,  true},
    {WATCH_APP_STAGE_RENDER,  "RENDER",  watch_app_run_render_stage,  WATCH_APP_STAGE_RENDER_BUDGET_MS,  WDT_CHECKPOINT_RENDER,  POWER_QOS_RENDER_DUE,                                   true,  true},
    {WATCH_APP_STAGE_IDLE,    "IDLE",    watch_app_run_idle_stage,    WATCH_APP_STAGE_IDLE_BUDGET_MS,    WDT_CHECKPOINT_IDLE,    POWER_QOS_NONE,                                         true,  false},
};

static uint8_t watch_app_runtime_stage_descriptor_count(void)
{
    return (uint8_t)(sizeof(g_watch_app_stage_descriptors) / sizeof(g_watch_app_stage_descriptors[0]));
}

uint8_t watch_app_stage_manifest_count(void)
{
    return watch_app_runtime_stage_descriptor_count();
}

static const WatchAppStageDescriptor *watch_app_runtime_stage_descriptor_by_id(WatchAppStageId id)
{
    for (uint8_t i = 0U; i < watch_app_runtime_stage_descriptor_count(); ++i) {
        if (g_watch_app_stage_descriptors[i].id == id) {
            return &g_watch_app_stage_descriptors[i];
        }
    }
    return NULL;
}

bool watch_app_stage_manifest_at(uint8_t index, WatchAppStageManifestEntry *out)
{
    const WatchAppStageDescriptor *descriptor;

    if (out == NULL || index >= watch_app_runtime_stage_descriptor_count()) {
        return false;
    }
    descriptor = &g_watch_app_stage_descriptors[index];
    out->id = descriptor->id;
    out->name = descriptor->name;
    out->budget_ms = descriptor->budget_ms;
    out->wdt_checkpoint = descriptor->wdt_checkpoint;
    out->qos_mask = descriptor->qos_mask;
    out->state_exposed = descriptor->state_exposed;
    out->transaction_pm_lock = descriptor->transaction_pm_lock;
    return true;
}

uint32_t watch_app_stage_budget_ms(WatchAppStageId id)
{
    const WatchAppStageDescriptor *descriptor = watch_app_runtime_stage_descriptor_by_id(id);
    return descriptor != NULL ? descriptor->budget_ms : 0U;
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

    for (uint8_t i = 0U; i < watch_app_runtime_stage_descriptor_count(); ++i) {
        const WatchAppStageDescriptor *descriptor = &g_watch_app_stage_descriptors[i];
        bool pm_lock_entered = false;
        if (descriptor->run_fn != NULL) {
            if (descriptor->transaction_pm_lock) {
                pm_lock_entered = power_policy_platform_pm_lock_enter(descriptor->qos_mask, descriptor->name);
            }
            descriptor->run_fn(state, now);
            if (pm_lock_entered && descriptor->transaction_pm_lock) {
                (void)power_policy_platform_pm_lock_exit(descriptor->qos_mask, descriptor->name);
            }
        }
    }
    model_flush_read_snapshots();
}

const char *watch_app_runtime_stage_name(WatchAppStageId id)
{
    for (uint8_t i = 0U; i < watch_app_runtime_stage_descriptor_count(); ++i) {
        if (g_watch_app_stage_descriptors[i].id == id) {
            return g_watch_app_stage_descriptors[i].name;
        }
    }
    return "UNKNOWN";
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


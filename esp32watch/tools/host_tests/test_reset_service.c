#include "services/reset_service.h"
#include "services/runtime_event_service.h"
#include "services/device_config_authority.h"
#include "model.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_model_restore_calls = 0U;
static uint32_t g_model_factory_restore_calls = 0U;
static uint32_t g_device_config_restore_calls = 0U;
static bool g_device_config_restore_ok = true;
static bool g_reload_ok = true;
static bool g_apply_committed = true;
static RuntimeReloadReport g_reload_report = {0};
static uint32_t g_audit_events_seen = 0U;
static uint32_t g_display_apply_calls = 0U;
static uint32_t g_sensor_set_sensitivity_calls = 0U;
static uint32_t g_sensor_clear_calls = 0U;
static uint32_t g_save_settings_calls = 0U;
static uint32_t g_save_alarms_calls = 0U;
static uint32_t g_save_game_stats_calls = 0U;
static uint32_t g_clear_calibration_calls = 0U;
static uint32_t g_request_commit_calls = 0U;
static uint32_t g_commit_now_calls = 0U;
static bool g_last_commit_ok = true;
static uint32_t g_runtime_request_flags = 0U;
static StorageCommitReason g_runtime_request_reason = STORAGE_COMMIT_REASON_NONE;
static ModelDomainState g_domain_state = {0};
static bool g_domain_state_ok = true;

static RuntimeEventSubscription make_subscription(RuntimeServiceEventHandler handler, void *ctx)
{
    RuntimeEventSubscription subscription = {
        .handler = handler,
        .ctx = ctx,
        .name = "audit",
        .priority = 0,
        .critical = true,
    };
    return subscription;
}

void model_restore_defaults(void)
{
    ++g_model_restore_calls;
}

void model_factory_reset_defaults(void)
{
    ++g_model_factory_restore_calls;
}

uint32_t model_consume_runtime_requests(StorageCommitReason *reason)
{
    if (reason != NULL) {
        *reason = g_runtime_request_reason;
    }
    return g_runtime_request_flags;
}

const ModelDomainState *model_get_domain_state(ModelDomainState *out)
{
    if (out == NULL || !g_domain_state_ok) {
        return NULL;
    }
    *out = g_domain_state;
    return out;
}

void display_service_apply_settings(const SettingsState *settings)
{
    assert(settings != NULL);
    ++g_display_apply_calls;
}

void sensor_service_set_sensitivity(uint8_t level)
{
    (void)level;
    ++g_sensor_set_sensitivity_calls;
}

void sensor_service_clear_calibration(void)
{
    ++g_sensor_clear_calls;
}

void storage_facade_clear_sensor_calibration(void)
{
    ++g_clear_calibration_calls;
}

void storage_facade_save_settings(const SettingsState *settings)
{
    assert(settings != NULL);
    ++g_save_settings_calls;
}

void storage_facade_save_alarms(const AlarmState *alarms, uint8_t count)
{
    assert(alarms != NULL);
    assert(count == APP_MAX_ALARMS);
    ++g_save_alarms_calls;
}

void storage_facade_save_game_stats(const GameStatsState *stats)
{
    assert(stats != NULL);
    ++g_save_game_stats_calls;
}

void storage_service_request_commit(StorageCommitReason reason)
{
    assert(reason == g_runtime_request_reason);
    ++g_request_commit_calls;
}

void storage_service_commit_now_with_reason(StorageCommitReason reason)
{
    assert(reason == g_runtime_request_reason);
    ++g_commit_now_calls;
}

bool storage_service_get_last_commit_ok(void)
{
    return g_last_commit_ok;
}

bool device_config_authority_restore_defaults(DeviceConfigAuthorityApplyReport *out)
{
    ++g_device_config_restore_calls;
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->committed = g_apply_committed && g_device_config_restore_ok;
        out->wifi_saved = true;
        out->network_saved = true;
        out->token_saved = true;
        out->wifi_changed = true;
        out->network_changed = true;
        out->token_changed = true;
        out->runtime_reload_requested = true;
        out->runtime_reload_ok = g_reload_ok;
        out->reload = g_reload_report;
    }
    return g_device_config_restore_ok && g_reload_ok;
}

static bool audit_handler(RuntimeServiceEvent event, void *ctx)
{
    (void)ctx;
    assert(event == RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED ||
           event == RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED ||
           event == RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED);
    ++g_audit_events_seen;
    return true;
}

static void seed_runtime_requests(uint32_t flags)
{
    memset(&g_domain_state, 0, sizeof(g_domain_state));
    g_domain_state.settings.sensor_sensitivity = 3U;
    g_runtime_request_flags = flags;
    g_runtime_request_reason = STORAGE_COMMIT_REASON_RESTORE_DEFAULTS;
    g_domain_state_ok = true;
    g_last_commit_ok = true;
    g_display_apply_calls = 0U;
    g_sensor_set_sensitivity_calls = 0U;
    g_sensor_clear_calls = 0U;
    g_save_settings_calls = 0U;
    g_save_alarms_calls = 0U;
    g_save_game_stats_calls = 0U;
    g_clear_calibration_calls = 0U;
    g_request_commit_calls = 0U;
    g_commit_now_calls = 0U;
}

int main(void)
{
    ResetActionReport report = {0};

    runtime_event_service_reset();
    { RuntimeEventSubscription subscription = make_subscription(audit_handler, NULL); assert(runtime_event_service_register_ex(&subscription)); }

    g_model_restore_calls = 0U;
    g_model_factory_restore_calls = 0U;
    g_device_config_restore_calls = 0U;
    g_audit_events_seen = 0U;
    seed_runtime_requests(MODEL_RUNTIME_REQUEST_STAGE_SETTINGS |
                          MODEL_RUNTIME_REQUEST_STAGE_ALARMS |
                          MODEL_RUNTIME_REQUEST_STORAGE_COMMIT |
                          MODEL_RUNTIME_REQUEST_APPLY_BRIGHTNESS |
                          MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS |
                          MODEL_RUNTIME_REQUEST_CLEAR_SENSOR_CALIBRATION);
    assert(reset_service_reset_app_state(&report));
    assert(report.kind == RESET_ACTION_APP_STATE);
    assert(report.app_state_reset);
    assert(!report.device_config_reset);
    assert(!report.runtime_reload_requested);
    assert(report.audit_notified);
    assert(g_model_restore_calls == 1U);
    assert(g_model_factory_restore_calls == 0U);
    assert(g_device_config_restore_calls == 0U);
    assert(g_display_apply_calls == 1U);
    assert(g_sensor_set_sensitivity_calls == 1U);
    assert(g_sensor_clear_calls == 1U);
    assert(g_save_settings_calls == 1U);
    assert(g_save_alarms_calls == 1U);
    assert(g_save_game_stats_calls == 0U);
    assert(g_clear_calibration_calls == 1U);
    assert(g_request_commit_calls == 1U);
    assert(g_commit_now_calls == 1U);
    assert(g_audit_events_seen == 1U);

    g_reload_ok = true;
    g_reload_report = (RuntimeReloadReport){
        .requested = true,
        .preflight_ok = true,
        .apply_attempted = true,
        .event_dispatch_ok = true,
        .verify_ok = true,
        .partial_success = false,
        .wifi_reload_ok = true,
        .network_reload_ok = true,
        .handler_count = 2U,
        .handler_success_count = 2U,
        .handler_failure_count = 0U,
        .handler_critical_failure_count = 0U,
        .first_failed_handler_name = "NONE",
        .failure_phase = "NONE",
        .failure_code = "NONE"
    };
    report = (ResetActionReport){0};
    assert(reset_service_reset_device_config(&report));
    assert(report.kind == RESET_ACTION_DEVICE_CONFIG);
    assert(!report.app_state_reset);
    assert(report.device_config_reset);
    assert(report.runtime_reload_requested);
    assert(report.runtime_reload_ok);
    assert(report.reload.handler_count == 2U);
    assert(report.audit_notified);

    g_device_config_restore_ok = true;
    g_reload_ok = false;
    g_reload_report = (RuntimeReloadReport){
        .requested = true,
        .preflight_ok = true,
        .apply_attempted = true,
        .event_dispatch_ok = false,
        .verify_ok = false,
        .partial_success = true,
        .wifi_reload_ok = true,
        .network_reload_ok = false,
        .handler_count = 2U,
        .handler_success_count = 1U,
        .handler_failure_count = 1U,
        .handler_critical_failure_count = 1U,
        .first_failed_handler_name = "network_sync",
        .failure_phase = "DISPATCH",
        .failure_code = "EVENT_DISPATCH_FAILED"
    };
    seed_runtime_requests(MODEL_RUNTIME_REQUEST_STAGE_SETTINGS |
                          MODEL_RUNTIME_REQUEST_STAGE_ALARMS |
                          MODEL_RUNTIME_REQUEST_STAGE_GAME_STATS |
                          MODEL_RUNTIME_REQUEST_STORAGE_COMMIT);
    report = (ResetActionReport){0};
    assert(!reset_service_factory_reset(&report));
    assert(report.kind == RESET_ACTION_FACTORY);
    assert(report.app_state_reset);
    assert(g_model_factory_restore_calls == 1U);
    assert(g_save_settings_calls == 1U);
    assert(g_save_alarms_calls == 1U);
    assert(g_save_game_stats_calls == 1U);
    assert(g_request_commit_calls == 1U);
    assert(g_commit_now_calls == 1U);
    assert(report.device_config_reset);
    assert(report.runtime_reload_requested);
    assert(!report.runtime_reload_ok);
    assert(report.audit_notified);

    seed_runtime_requests(MODEL_RUNTIME_REQUEST_STAGE_SETTINGS | MODEL_RUNTIME_REQUEST_STORAGE_COMMIT);
    g_last_commit_ok = false;
    report = (ResetActionReport){0};
    assert(!reset_service_reset_app_state(&report));
    assert(!report.app_state_reset);
    assert(g_commit_now_calls == 1U);

    g_model_restore_calls = 0U;
    g_model_factory_restore_calls = 0U;
    g_device_config_restore_ok = false;
    g_reload_ok = true;
    report = (ResetActionReport){0};
    assert(!reset_service_reset_device_config(&report));
    assert(report.kind == RESET_ACTION_DEVICE_CONFIG);
    assert(!report.device_config_reset);
    assert(!report.audit_notified);

    g_model_restore_calls = 0U;
    g_device_config_restore_calls = 0U;
    report = (ResetActionReport){0};
    assert(!reset_service_factory_reset(&report));
    assert(report.kind == RESET_ACTION_FACTORY);
    assert(!report.app_state_reset);
    assert(!report.device_config_reset);
    assert(g_model_restore_calls == 0U);
    assert(g_model_factory_restore_calls == 0U);
    assert(g_device_config_restore_calls == 1U);

    seed_runtime_requests(MODEL_RUNTIME_REQUEST_STAGE_SETTINGS | MODEL_RUNTIME_REQUEST_STORAGE_COMMIT);
    g_domain_state_ok = false;
    report = (ResetActionReport){0};
    assert(!reset_service_reset_app_state(&report));
    assert(!report.app_state_reset);

    puts("[OK] reset_service behavior check passed");
    return 0;
}

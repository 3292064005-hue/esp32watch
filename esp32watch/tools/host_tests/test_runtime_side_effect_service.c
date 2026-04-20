#include "services/runtime_side_effect_service.h"
#include "model.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_runtime_request_flags = 0U;
static StorageCommitReason g_runtime_request_reason = STORAGE_COMMIT_REASON_NONE;
static ModelDomainState g_domain_state = {0};
static bool g_domain_state_ok = true;
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
static uint8_t g_last_sensor_level = 0U;

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
    g_last_sensor_level = level;
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

static void reset_counters(void)
{
    g_display_apply_calls = 0U;
    g_sensor_set_sensitivity_calls = 0U;
    g_sensor_clear_calls = 0U;
    g_save_settings_calls = 0U;
    g_save_alarms_calls = 0U;
    g_save_game_stats_calls = 0U;
    g_clear_calibration_calls = 0U;
    g_request_commit_calls = 0U;
    g_commit_now_calls = 0U;
    g_last_sensor_level = 0U;
    g_last_commit_ok = true;
    g_domain_state_ok = true;
    memset(&g_domain_state, 0, sizeof(g_domain_state));
    g_domain_state.settings.sensor_sensitivity = 7U;
    g_runtime_request_reason = STORAGE_COMMIT_REASON_RESTORE_DEFAULTS;
}

int main(void)
{
    RuntimeSideEffectReport report = {0};
    uint8_t last_sensor_sensitivity = 0U;

    reset_counters();
    g_runtime_request_flags = MODEL_RUNTIME_REQUEST_STAGE_SETTINGS |
                              MODEL_RUNTIME_REQUEST_STAGE_ALARMS |
                              MODEL_RUNTIME_REQUEST_STAGE_GAME_STATS |
                              MODEL_RUNTIME_REQUEST_STORAGE_COMMIT |
                              MODEL_RUNTIME_REQUEST_APPLY_BRIGHTNESS |
                              MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS |
                              MODEL_RUNTIME_REQUEST_CLEAR_SENSOR_CALIBRATION;
    assert(runtime_side_effect_service_apply_pending(RUNTIME_SIDE_EFFECT_CONTEXT_NORMAL,
                                                     RUNTIME_SIDE_EFFECT_COMMIT_DEFERRED,
                                                     &last_sensor_sensitivity,
                                                     &report));
    assert(report.context == RUNTIME_SIDE_EFFECT_CONTEXT_NORMAL);
    assert(report.commit_policy == RUNTIME_SIDE_EFFECT_COMMIT_DEFERRED);
    assert(report.domain_state_loaded);
    assert(report.consumed_flags == g_runtime_request_flags);
    assert(report.commit_requested);
    assert(!report.commit_executed);
    assert(report.commit_ok);
    assert(g_display_apply_calls == 1U);
    assert(g_sensor_set_sensitivity_calls == 1U);
    assert(g_sensor_clear_calls == 1U);
    assert(g_save_settings_calls == 1U);
    assert(g_save_alarms_calls == 1U);
    assert(g_save_game_stats_calls == 1U);
    assert(g_clear_calibration_calls == 1U);
    assert(g_request_commit_calls == 1U);
    assert(g_commit_now_calls == 0U);
    assert(last_sensor_sensitivity == 7U);
    assert(g_last_sensor_level == 7U);

    reset_counters();
    g_runtime_request_flags = MODEL_RUNTIME_REQUEST_STORAGE_COMMIT |
                              MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS;
    assert(runtime_side_effect_service_apply_pending(RUNTIME_SIDE_EFFECT_CONTEXT_RESET,
                                                     RUNTIME_SIDE_EFFECT_COMMIT_IMMEDIATE,
                                                     &last_sensor_sensitivity,
                                                     &report));
    assert(report.context == RUNTIME_SIDE_EFFECT_CONTEXT_RESET);
    assert(report.commit_requested);
    assert(report.commit_executed);
    assert(report.commit_ok);
    assert(g_request_commit_calls == 1U);
    assert(g_commit_now_calls == 1U);
    assert(g_sensor_set_sensitivity_calls == 1U);

    reset_counters();
    g_runtime_request_flags = MODEL_RUNTIME_REQUEST_STAGE_SETTINGS;
    g_domain_state_ok = false;
    assert(!runtime_side_effect_service_apply_pending(RUNTIME_SIDE_EFFECT_CONTEXT_FACTORY,
                                                      RUNTIME_SIDE_EFFECT_COMMIT_IMMEDIATE,
                                                      &last_sensor_sensitivity,
                                                      &report));
    assert(!report.domain_state_loaded);
    assert(!report.commit_ok);
    assert(g_save_settings_calls == 0U);

    reset_counters();
    g_runtime_request_flags = MODEL_RUNTIME_REQUEST_NONE;
    assert(runtime_side_effect_service_apply_pending(RUNTIME_SIDE_EFFECT_CONTEXT_NORMAL,
                                                     RUNTIME_SIDE_EFFECT_COMMIT_DEFERRED,
                                                     &last_sensor_sensitivity,
                                                     &report));
    assert(report.consumed_flags == 0U);
    assert(!report.commit_requested);

    puts("[OK] runtime_side_effect_service behavior check passed");
    return 0;
}

#include "model_internal.h"
#include "app_config.h"

static void request_manual_settings_commit(uint32_t extra_flags)
{
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STORAGE_COMMIT | extra_flags,
                                        STORAGE_COMMIT_REASON_MANUAL);
}

void model_set_watchface(uint8_t face)
{
    g_model_domain_state.settings.watchface = face > 2U ? 2U : face;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_cycle_watchface(int8_t dir)
{
    int8_t next = (int8_t)g_model_domain_state.settings.watchface + dir;
    if (next < 0) next = 2;
    if (next > 2) next = 0;
    g_model_domain_state.settings.watchface = (uint8_t)next;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_set_brightness(uint8_t level)
{
    g_model_domain_state.settings.brightness = level > 3U ? 3U : level;
    model_internal_persist_settings();
    request_manual_settings_commit(MODEL_RUNTIME_REQUEST_APPLY_BRIGHTNESS);
    model_internal_commit_domain_mutation();
}

void model_set_vibrate(bool enabled)
{
#if APP_FEATURE_VIBRATION
    g_model_domain_state.settings.vibrate = enabled;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
#else
    (void)enabled;
    if (g_model_domain_state.settings.vibrate) {
        g_model_domain_state.settings.vibrate = false;
        model_internal_persist_settings();
        request_manual_settings_commit(0U);
        model_internal_commit_domain_mutation();
    }
#endif
}

void model_set_auto_wake(bool enabled)
{
    g_model_domain_state.settings.auto_wake = enabled;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_set_auto_sleep(bool enabled)
{
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)enabled;
    g_model_domain_state.settings.auto_sleep = false;
#else
    g_model_domain_state.settings.auto_sleep = enabled;
#endif
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_set_dnd(bool enabled)
{
    g_model_domain_state.settings.dnd = enabled;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_set_goal(uint32_t goal)
{
    goal = APP_CLAMP(goal, 1000U, 30000U);
    g_model_domain_state.activity.goal = goal;
    g_model_domain_state.settings.goal = goal;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_set_show_seconds(bool enabled)
{
    g_model_domain_state.settings.show_seconds = enabled;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_set_animations(bool enabled)
{
    g_model_domain_state.settings.animations = enabled;
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
}

void model_set_sensor_sensitivity(uint8_t level)
{
    g_model_domain_state.settings.sensor_sensitivity = level > 2U ? 2U : level;
    model_internal_persist_settings();
    request_manual_settings_commit(MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS);
    model_internal_commit_domain_mutation();
}

void model_set_screen_timeout_idx(uint8_t idx)
{
    g_model_domain_state.settings.screen_timeout_idx = idx > 2U ? 2U : idx;
    g_model_runtime_state.screen_timeout_ms = model_internal_timeout_idx_to_ms(g_model_domain_state.settings.screen_timeout_idx);
    model_internal_persist_settings();
    request_manual_settings_commit(0U);
    model_internal_commit_domain_mutation();
    model_internal_commit_runtime_mutation();
}

void model_cycle_screen_timeout(int8_t dir)
{
    int8_t idx = (int8_t)g_model_domain_state.settings.screen_timeout_idx + dir;
    if (idx < 0) idx = 2;
    if (idx > 2) idx = 0;
    model_set_screen_timeout_idx((uint8_t)idx);
}

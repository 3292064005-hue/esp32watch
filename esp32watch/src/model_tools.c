#include "model_internal.h"
#include "app_config.h"
#include <string.h>

void model_stopwatch_toggle(uint32_t now_ms)
{
    if (!g_model.stopwatch.running) {
        g_model.stopwatch.running = true;
        g_model.stopwatch.last_tick_ms = now_ms;
    } else {
        g_model.stopwatch.running = false;
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_stopwatch_reset(void)
{
    if (!g_model.stopwatch.running) {
        memset(&g_model.stopwatch, 0, sizeof(g_model.stopwatch));
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_stopwatch_lap(void)
{
    if (!g_model.stopwatch.running) {
        return;
    }
    if (g_model.stopwatch.lap_count < APP_STOPWATCH_LAP_MAX) {
        g_model.stopwatch.laps[g_model.stopwatch.lap_count++] = g_model.stopwatch.elapsed_ms;
    } else {
        memmove(&g_model.stopwatch.laps[0], &g_model.stopwatch.laps[1], sizeof(uint32_t) * (APP_STOPWATCH_LAP_MAX - 1U));
        g_model.stopwatch.laps[APP_STOPWATCH_LAP_MAX - 1U] = g_model.stopwatch.elapsed_ms;
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_timer_toggle(uint32_t now_ms)
{
    if (!g_model.timer.running) {
        if (g_model.timer.remain_s == 0U) {
            g_model.timer.remain_s = g_model.timer.preset_s;
        }
        g_model.timer.running = true;
        g_model.timer.last_tick_ms = now_ms;
    } else {
        g_model.timer.running = false;
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_timer_reset(void)
{
    if (!g_model.timer.running) {
        g_model.timer.remain_s = g_model.timer.preset_s;
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_timer_set_preset(uint32_t preset_s)
{
    if (preset_s < 10U) preset_s = 10U;
    if (preset_s > 35999U) preset_s = 35999U;
    g_model.timer.preset_s = preset_s;
    if (!g_model.timer.running) {
        g_model.timer.remain_s = preset_s;
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

void model_timer_adjust_seconds(int32_t delta_s)
{
    int32_t value;
    if (g_model.timer.running) {
        return;
    }
    value = (int32_t)g_model.timer.preset_s + delta_s;
    value = APP_CLAMP(value, 10, 35999);
    model_timer_set_preset((uint32_t)value);
}

void model_timer_cycle_preset(int8_t dir)
{
    static const uint16_t presets[APP_TIMER_PRESET_COUNT] = {60U, 180U, 300U, 600U, 1500U};
    int8_t idx = (int8_t)g_model.timer.preset_index + dir;
    if (idx < 0) idx = APP_TIMER_PRESET_COUNT - 1;
    if (idx >= (int8_t)APP_TIMER_PRESET_COUNT) idx = 0;
    g_model.timer.preset_index = (uint8_t)idx;
    model_timer_set_preset(presets[g_model.timer.preset_index]);
}

#include "model_internal.h"
#include "app_config.h"
#include "sensor.h"
#include "platform_api.h"
#include <stddef.h>

void model_set_battery(uint16_t mv, uint8_t percent, bool charging, bool present)
{
    model_runtime_apply_battery_snapshot(mv, percent, charging, present);
}

void model_update_sensor_raw(const SensorSnapshot *snap, uint32_t now_ms)
{
    (void)now_ms;
    model_runtime_apply_sensor_snapshot(snap);
}

void model_update_activity(bool wrist_raise, uint8_t activity_level,
                           MotionState motion_state, uint16_t steps_inc,
                           uint32_t now_ms)
{
    g_model_domain_state.activity.wrist_raise = wrist_raise;
    g_model_domain_state.activity.activity_level = activity_level;
    g_model_domain_state.activity.motion_state = motion_state;
    if (motion_state != MOTION_STATE_IDLE) {
        g_model_runtime_state.sensor.last_motion_ms = now_ms;
    }

    if (steps_inc > 0U) {
        g_model_domain_state.activity.steps += steps_inc;
        g_model_runtime_state.sensor.steps_total += steps_inc;
        g_model_runtime_state.last_user_activity_ms = now_ms;
    }
    model_internal_commit_domain_mutation();
    model_internal_commit_runtime_mutation();
}

void model_note_user_activity(void)
{
    model_runtime_note_user_activity(platform_time_now_ms());
}

void model_note_render(uint32_t now_ms)
{
    model_runtime_note_render(now_ms);
}

void model_note_manual_wake(uint32_t now_ms)
{
    model_runtime_note_manual_wake(now_ms);
}

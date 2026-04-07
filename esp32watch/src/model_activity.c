#include "model_internal.h"
#include "app_config.h"
#include "sensor.h"
#include "platform_api.h"
#include <stddef.h>

void model_set_battery(uint16_t mv, uint8_t percent, bool charging, bool present)
{
    g_model.battery_mv = mv;
    g_model.battery_percent = APP_CLAMP(percent, 0U, 100U);
    g_model.charging = charging;
    g_model.battery_present = present;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_update_sensor_raw(const SensorSnapshot *snap, uint32_t now_ms)
{
    (void)now_ms;
    if (snap == NULL) {
        return;
    }

    g_model.sensor.online = snap->online;
    g_model.sensor.calibrated = snap->calibrated;
    g_model.sensor.calibration_pending = snap->calibration_pending;
    g_model.sensor.runtime_state = (uint8_t)snap->runtime_state;
    g_model.sensor.calibration_state = (uint8_t)snap->calibration_state;
    g_model.sensor.static_now = snap->static_now;
    g_model.sensor.ax = snap->ax;
    g_model.sensor.ay = snap->ay;
    g_model.sensor.az = snap->az;
    g_model.sensor.gx = snap->gx;
    g_model.sensor.gy = snap->gy;
    g_model.sensor.gz = snap->gz;
    g_model.sensor.accel_norm_mg = snap->accel_norm_mg;
    g_model.sensor.baseline_mg = snap->baseline_mg;
    g_model.sensor.motion_score = snap->motion_score;
    g_model.sensor.pitch_deg = snap->pitch_deg;
    g_model.sensor.roll_deg = snap->roll_deg;
    g_model.sensor.quality = snap->quality;
    g_model.sensor.calibration_progress = snap->calibration_progress;
    g_model.sensor.fault_count = snap->fault_count;
    g_model.sensor.reinit_count = snap->reinit_count;
    g_model.sensor.error_code = snap->error_code;
    g_model.sensor.last_sample_ms = snap->last_sample_ms;
    g_model.sensor.offline_since_ms = snap->offline_since_ms;
    g_model.sensor.steps_total = snap->steps_total;
    g_model.sensor.retry_backoff_ms = snap->retry_backoff_ms;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_update_activity(bool wrist_raise, uint8_t activity_level,
                           MotionState motion_state, uint16_t steps_inc,
                           uint32_t now_ms)
{
    g_model.activity.wrist_raise = wrist_raise;
    g_model.activity.activity_level = activity_level;
    g_model.activity.motion_state = motion_state;
    if (motion_state != MOTION_STATE_IDLE) {
        g_model.sensor.last_motion_ms = now_ms;
    }

    if (steps_inc > 0U) {
        g_model.activity.steps += steps_inc;
        g_model.sensor.steps_total += steps_inc;
        g_model.last_user_activity_ms = now_ms;
    }
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_note_user_activity(void)
{
    g_model.last_user_activity_ms = platform_time_now_ms();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_note_render(uint32_t now_ms)
{
    g_model.last_render_ms = now_ms;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_note_manual_wake(uint32_t now_ms)
{
    g_model.last_wake_ms = now_ms;
    g_model.last_user_activity_ms = now_ms;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}


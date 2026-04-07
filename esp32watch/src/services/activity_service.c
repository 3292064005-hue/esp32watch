#include "services/activity_service.h"
#include "app_tuning.h"
#include "app_config.h"
#include "services/activity_model_adapter.h"
#include "sensor_profile.h"
#include <string.h>
#include <stddef.h>

typedef enum {
    WRIST_ARM_DOWN = 0,
    WRIST_CANDIDATE,
    WRIST_CONFIRMED,
    WRIST_COOLDOWN
} WristState;

typedef struct {
    bool above_high;
    uint8_t last_reinit_count;
    uint32_t last_processed_update_seq;
    uint32_t last_step_ms;
    uint32_t last_motion_ms;
    uint32_t wrist_candidate_ms;
    uint32_t wrist_cooldown_until_ms;
    uint32_t warmup_until_ms;
    WristState wrist_state;
} ActivityServiceState;

typedef struct {
    MotionState motion_state;
    uint8_t activity_level;
    bool warmup_done;
    bool quality_ok;
    bool gait_ok;
} ActivityMotionPhase;

typedef struct {
    uint16_t steps_inc;
    bool cadence_ok;
} ActivityStepPhase;

typedef struct {
    bool wrist_candidate;
    bool wrist_raise;
} ActivityWristPhase;

static ActivityServiceState g_activity_service;
static bool g_activity_service_initialized;

static int16_t abs16_local(int16_t v)
{
    return v < 0 ? (int16_t)(-v) : v;
}

static MotionState derive_motion_state(int16_t motion_score, MotionState previous)
{
    switch (previous) {
        case MOTION_STATE_ACTIVE:
            if (motion_score >= 220) return MOTION_STATE_ACTIVE;
            if (motion_score >= 130) return MOTION_STATE_WALKING;
            if (motion_score >= 70) return MOTION_STATE_LIGHT;
            return MOTION_STATE_IDLE;
        case MOTION_STATE_WALKING:
            if (motion_score >= 280) return MOTION_STATE_ACTIVE;
            if (motion_score >= 120) return MOTION_STATE_WALKING;
            if (motion_score >= 60) return MOTION_STATE_LIGHT;
            return MOTION_STATE_IDLE;
        case MOTION_STATE_LIGHT:
            if (motion_score >= 280) return MOTION_STATE_ACTIVE;
            if (motion_score >= 150) return MOTION_STATE_WALKING;
            if (motion_score >= 55) return MOTION_STATE_LIGHT;
            return MOTION_STATE_IDLE;
        default:
            if (motion_score >= 280) return MOTION_STATE_ACTIVE;
            if (motion_score >= 150) return MOTION_STATE_WALKING;
            if (motion_score >= 70) return MOTION_STATE_LIGHT;
            return MOTION_STATE_IDLE;
    }
}

static uint8_t motion_state_to_level(MotionState state)
{
    switch (state) {
        case MOTION_STATE_LIGHT: return 1U;
        case MOTION_STATE_WALKING: return 2U;
        case MOTION_STATE_ACTIVE: return 3U;
        default: return 0U;
    }
}

static void reset_runtime_after_reinit(uint32_t now_ms, uint8_t reinit_count, const SensorProfile *profile)
{
    g_activity_service.above_high = false;
    g_activity_service.last_reinit_count = reinit_count;
    g_activity_service.last_step_ms = 0U;
    g_activity_service.last_motion_ms = 0U;
    g_activity_service.wrist_candidate_ms = 0U;
    g_activity_service.wrist_cooldown_until_ms = 0U;
    g_activity_service.wrist_state = WRIST_ARM_DOWN;
    g_activity_service.warmup_until_ms = now_ms + profile->warmup_ms;
}

static ActivityMotionPhase activity_compute_motion_phase(const SensorSnapshot *snap,
                                                         uint32_t now_ms,
                                                         const SensorProfile *profile,
                                                         MotionState previous)
{
    ActivityMotionPhase phase;

    memset(&phase, 0, sizeof(phase));
    phase.motion_state = derive_motion_state(snap->motion_score, previous);
    if (phase.motion_state != MOTION_STATE_IDLE) {
        g_activity_service.last_motion_ms = now_ms;
    } else if ((now_ms - g_activity_service.last_motion_ms) <= app_tuning_manifest_get()->sensor_active_decay_ms) {
        phase.motion_state = MOTION_STATE_LIGHT;
    }
    phase.activity_level = motion_state_to_level(phase.motion_state);
    phase.warmup_done = now_ms >= g_activity_service.warmup_until_ms;
    phase.quality_ok = snap->quality >= profile->min_quality;
    phase.gait_ok = (phase.motion_state >= MOTION_STATE_WALKING) && !snap->static_now;
    return phase;
}

static ActivityStepPhase activity_compute_step_phase(const SensorSnapshot *snap,
                                                     uint32_t now_ms,
                                                     const SensorProfile *profile,
                                                     const ActivityMotionPhase *motion_phase)
{
    ActivityStepPhase phase;
    int16_t dynamic;

    memset(&phase, 0, sizeof(phase));
    dynamic = (int16_t)(snap->accel_norm_mg - snap->baseline_mg);

    if (dynamic > profile->step_high_threshold_mg) {
        g_activity_service.above_high = true;
        return phase;
    }

    if (!g_activity_service.above_high || dynamic >= profile->step_low_threshold_mg) {
        return phase;
    }

    g_activity_service.above_high = false;
    if (g_activity_service.last_step_ms == 0U) {
        phase.cadence_ok = true;
    } else {
        uint32_t dt = now_ms - g_activity_service.last_step_ms;
        phase.cadence_ok = (dt >= app_tuning_manifest_get()->sensor_step_min_interval_ms && dt <= app_tuning_manifest_get()->sensor_step_max_interval_ms);
    }

    if (motion_phase->warmup_done && snap->calibrated && phase.cadence_ok && motion_phase->quality_ok && motion_phase->gait_ok) {
        phase.steps_inc = 1U;
        g_activity_service.last_step_ms = now_ms;
    } else if (g_activity_service.last_step_ms == 0U) {
        g_activity_service.last_step_ms = now_ms;
    }

    return phase;
}

static ActivityWristPhase activity_compute_wrist_phase(const SensorSnapshot *snap,
                                                       uint32_t now_ms,
                                                       const SensorProfile *profile,
                                                       const ActivityMotionPhase *motion_phase)
{
    ActivityWristPhase phase;
    bool wrist_motion_ok;
    int16_t deviation;

    memset(&phase, 0, sizeof(phase));
    deviation = abs16_local((int16_t)(snap->accel_norm_mg - snap->baseline_mg));
    wrist_motion_ok = abs16_local(snap->gy) < app_tuning_manifest_get()->sensor_wrist_gyro_threshold;

    phase.wrist_candidate = motion_phase->warmup_done &&
                            (motion_phase->motion_state >= MOTION_STATE_LIGHT) &&
                            motion_phase->quality_ok &&
                            (snap->pitch_deg > app_tuning_manifest_get()->sensor_wrist_min_pitch) &&
                            (snap->pitch_deg < app_tuning_manifest_get()->sensor_wrist_max_pitch) &&
                            (abs16_local(snap->roll_deg) < app_tuning_manifest_get()->sensor_wrist_max_roll) &&
                            wrist_motion_ok &&
                            (deviation > profile->wrist_threshold_mg);

    switch (g_activity_service.wrist_state) {
        case WRIST_ARM_DOWN:
            if (now_ms >= g_activity_service.wrist_cooldown_until_ms && phase.wrist_candidate) {
                g_activity_service.wrist_state = WRIST_CANDIDATE;
                g_activity_service.wrist_candidate_ms = now_ms;
            }
            break;
        case WRIST_CANDIDATE:
            if (!phase.wrist_candidate) {
                g_activity_service.wrist_state = WRIST_ARM_DOWN;
            } else if ((now_ms - g_activity_service.wrist_candidate_ms) >= 60U) {
                g_activity_service.wrist_state = WRIST_CONFIRMED;
            }
            break;
        case WRIST_CONFIRMED:
            phase.wrist_raise = true;
            g_activity_service.wrist_state = WRIST_COOLDOWN;
            g_activity_service.wrist_cooldown_until_ms = now_ms + app_tuning_manifest_get()->sensor_wrist_cooldown_ms;
            break;
        case WRIST_COOLDOWN:
            if (now_ms >= g_activity_service.wrist_cooldown_until_ms) {
                g_activity_service.wrist_state = phase.wrist_candidate ? WRIST_CANDIDATE : WRIST_ARM_DOWN;
                g_activity_service.wrist_candidate_ms = now_ms;
            }
            break;
        default:
            g_activity_service.wrist_state = WRIST_ARM_DOWN;
            break;
    }

    return phase;
}

void activity_service_init(void)
{
    memset(&g_activity_service, 0, sizeof(g_activity_service));
    g_activity_service_initialized = true;
}

void activity_service_ingest_snapshot(const SensorSnapshot *snap, uint32_t now_ms)
{
    ModelDomainState domain_state;
    const SensorProfile *profile;
    ActivityMotionPhase motion_phase;
    ActivityStepPhase step_phase;
    ActivityWristPhase wrist_phase;

    if (snap == NULL) {
        return;
    }

    if (snap->update_seq == 0U || snap->update_seq == g_activity_service.last_processed_update_seq) {
        return;
    }
    g_activity_service.last_processed_update_seq = snap->update_seq;

    if (activity_model_adapter_get_domain_state(&domain_state) == NULL) {
        return;
    }

    profile = sensor_profile_get(domain_state.settings.sensor_sensitivity);
    activity_model_adapter_update_sensor_raw(snap, now_ms);

    if (snap->reinit_count != g_activity_service.last_reinit_count) {
        reset_runtime_after_reinit(now_ms, snap->reinit_count, profile);
    }

    if (snap->runtime_state != SENSOR_STATE_READY &&
        snap->runtime_state != SENSOR_STATE_CALIBRATING) {
        activity_model_adapter_update_activity(false, 0U, MOTION_STATE_IDLE, 0U, now_ms);
        return;
    }

    motion_phase = activity_compute_motion_phase(snap, now_ms, profile, domain_state.activity.motion_state);
    step_phase = activity_compute_step_phase(snap, now_ms, profile, &motion_phase);
    wrist_phase = activity_compute_wrist_phase(snap, now_ms, profile, &motion_phase);

    activity_model_adapter_update_activity(wrist_phase.wrist_raise,
                          motion_phase.activity_level,
                          motion_phase.motion_state,
                          step_phase.steps_inc,
                          now_ms);
}


bool activity_service_is_initialized(void)
{
    return g_activity_service_initialized;
}

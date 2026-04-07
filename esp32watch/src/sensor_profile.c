#include "sensor_profile.h"
#include "app_tuning.h"

const SensorProfile *sensor_profile_get(uint8_t level)
{
    static SensorProfile profiles[3];
    const AppTuningManifest *tuning = app_tuning_manifest_get();
    uint8_t i;

    for (i = 0U; i < 3U; ++i) {
        profiles[i].step_high_threshold_mg = tuning->sensor_profile_step_high_threshold_mg[i];
        profiles[i].step_low_threshold_mg = tuning->sensor_profile_step_low_threshold_mg[i];
        profiles[i].wrist_threshold_mg = tuning->sensor_profile_wrist_threshold_mg[i];
        profiles[i].warmup_ms = tuning->sensor_profile_warmup_ms[i];
        profiles[i].min_quality = tuning->sensor_profile_min_quality[i];
    }

    if (level >= 3U) {
        level = 1U;
    }
    return &profiles[level];
}

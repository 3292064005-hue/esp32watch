#ifndef SENSOR_PROFILE_H
#define SENSOR_PROFILE_H

#include <stdint.h>

typedef struct {
    int16_t step_high_threshold_mg;
    int16_t step_low_threshold_mg;
    int16_t wrist_threshold_mg;
    uint16_t warmup_ms;
    uint8_t min_quality;
} SensorProfile;

const SensorProfile *sensor_profile_get(uint8_t level);

#endif

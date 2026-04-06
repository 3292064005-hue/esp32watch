#include "services/activity_model_adapter.h"

const ModelDomainState *activity_model_adapter_get_domain_state(ModelDomainState *out)
{
    return model_get_domain_state(out);
}

void activity_model_adapter_update_sensor_raw(const SensorSnapshot *snap, uint32_t now_ms)
{
    model_update_sensor_raw(snap, now_ms);
}

void activity_model_adapter_update_activity(bool wrist_raise,
                                            uint8_t activity_level,
                                            MotionState motion_state,
                                            uint16_t steps_inc,
                                            uint32_t now_ms)
{
    model_update_activity(wrist_raise, activity_level, motion_state, steps_inc, now_ms);
}

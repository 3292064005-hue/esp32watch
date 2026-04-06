#ifndef ACTIVITY_MODEL_ADAPTER_H
#define ACTIVITY_MODEL_ADAPTER_H

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the current domain-state snapshot required by the activity service.
 *
 * @param[out] out Destination snapshot. Must not be NULL.
 * @return Pointer to @p out when the snapshot was produced; NULL when the adapter contract cannot be satisfied.
 * @throws None.
 */
const ModelDomainState *activity_model_adapter_get_domain_state(ModelDomainState *out);

/**
 * @brief Forward a raw sensor snapshot into the model layer through a stable adapter boundary.
 *
 * @param[in] snap Sensor snapshot to mirror into runtime state. Must not be NULL.
 * @param[in] now_ms Monotonic tick when the snapshot is being applied.
 * @return void
 * @throws None.
 */
void activity_model_adapter_update_sensor_raw(const SensorSnapshot *snap, uint32_t now_ms);

/**
 * @brief Forward derived activity outputs into the model layer through a stable adapter boundary.
 *
 * @param[in] wrist_raise Whether this snapshot confirmed a wrist-raise event.
 * @param[in] activity_level Derived activity level.
 * @param[in] motion_state Derived motion state.
 * @param[in] steps_inc Number of steps contributed by this snapshot.
 * @param[in] now_ms Monotonic tick when the update is being applied.
 * @return void
 * @throws None.
 */
void activity_model_adapter_update_activity(bool wrist_raise,
                                            uint8_t activity_level,
                                            MotionState motion_state,
                                            uint16_t steps_inc,
                                            uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

#include "model_internal.h"
#include "services/time_service.h"

uint32_t model_datetime_to_epoch(const DateTime *dt)
{
    return time_service_datetime_to_epoch(dt);
}

void model_epoch_to_datetime(uint32_t epoch, DateTime *out)
{
    time_service_epoch_to_datetime(epoch, out);
}

void model_set_datetime(const DateTime *dt)
{
    TimeState next_state = (g_model.time_state == TIME_STATE_UNSET) ? TIME_STATE_RECOVERED : TIME_STATE_VALID;
    if (!time_service_try_set_datetime(dt, TIME_SOURCE_HOST_SYNC)) {
        return;
    }
    time_service_refresh(&g_model.now);
    g_model.current_day_id = time_service_day_id_from_epoch(time_service_get_epoch());
    g_model.time_state = next_state;
    g_model.time_valid = true;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

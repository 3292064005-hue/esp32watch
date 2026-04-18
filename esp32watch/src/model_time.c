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
    TimeSourceSnapshot source_snapshot = {0};

    if (!time_service_try_set_datetime(dt, TIME_SOURCE_HOST_SYNC)) {
        return;
    }
    (void)time_service_get_datetime_snapshot(&g_model_domain_state.now, &source_snapshot);
    g_model_domain_state.current_day_id = time_service_day_id_from_epoch(source_snapshot.epoch);
    g_model_domain_state.time_state = TIME_STATE_VALID;
    g_model_domain_state.time_valid = true;
    model_internal_commit_domain_mutation();
}

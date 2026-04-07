#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

typedef enum {
    TIME_SOURCE_RTC = 0,
    TIME_SOURCE_HOST_SYNC,
    TIME_SOURCE_NETWORK_SYNC,
    TIME_SOURCE_COMPANION_SYNC,
    TIME_SOURCE_RECOVERY
} TimeSourceType;

typedef struct {
    TimeSourceType source;
    uint32_t epoch;
    bool valid;
    uint32_t updated_at_ms;
} TimeSourceSnapshot;

void time_service_init(void);
uint32_t time_service_get_epoch(void);
void time_service_refresh(DateTime *out);
void time_service_set_datetime(const DateTime *dt);
bool time_service_try_set_datetime(const DateTime *dt, TimeSourceType source);
bool time_service_set_epoch_from_source(uint32_t epoch, TimeSourceType source);
void time_service_note_recovery_epoch(uint32_t epoch, bool valid);
bool time_service_get_source_snapshot(TimeSourceSnapshot *out);
const char *time_service_source_name(TimeSourceType source);
bool time_service_is_datetime_valid(const DateTime *dt);
uint32_t time_service_datetime_to_epoch(const DateTime *dt);
void time_service_epoch_to_datetime(uint32_t epoch, DateTime *out);
uint16_t time_service_day_id_from_epoch(uint32_t epoch);
bool time_service_is_reasonable_epoch(uint32_t epoch);

#endif

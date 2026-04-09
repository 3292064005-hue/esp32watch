#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TIME_SOURCE_RTC = 0,
    TIME_SOURCE_HOST_SYNC,
    TIME_SOURCE_NETWORK_SYNC,
    TIME_SOURCE_COMPANION_SYNC,
    TIME_SOURCE_RECOVERY
} TimeSourceType;

typedef enum {
    TIME_CONFIDENCE_NONE = 0,
    TIME_CONFIDENCE_LOW,
    TIME_CONFIDENCE_MEDIUM,
    TIME_CONFIDENCE_HIGH
} TimeConfidenceLevel;

typedef struct {
    TimeSourceType source;
    TimeConfidenceLevel confidence;
    uint32_t epoch;
    bool valid;
    bool authoritative;
    uint32_t updated_at_ms;
    uint32_t source_age_ms;
} TimeSourceSnapshot;

/**
 * @brief Initialize the time service recovery baseline and source snapshot.
 *
 * The service loads any persisted recovery epoch, derives the initial runtime epoch,
 * and records a low-confidence recovery source snapshot for later authoritative sync.
 *
 * @param None.
 * @return true when the service initialized and seeded its runtime snapshot.
 * @throws None.
 */
bool time_service_init(void);
bool time_service_is_datetime_valid(const DateTime *dt);
uint32_t time_service_datetime_to_epoch(const DateTime *dt);
void time_service_epoch_to_datetime(uint32_t epoch, DateTime *out);
uint32_t time_service_get_epoch(void);
void time_service_refresh(DateTime *out);
bool time_service_set_epoch_from_source(uint32_t epoch, TimeSourceType source);
bool time_service_try_set_datetime(const DateTime *dt, TimeSourceType source);
void time_service_set_datetime(const DateTime *dt);
void time_service_note_recovery_epoch(uint32_t epoch, bool valid);
bool time_service_get_source_snapshot(TimeSourceSnapshot *out);
const char *time_service_source_name(TimeSourceType source);
const char *time_service_confidence_name(TimeConfidenceLevel confidence);
uint16_t time_service_day_id_from_epoch(uint32_t epoch);
bool time_service_is_reasonable_epoch(uint32_t epoch);

#ifdef __cplusplus
}
#endif

#endif

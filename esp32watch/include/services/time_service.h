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
    uint32_t epoch;
    bool valid;
    uint32_t updated_at_ms;
    uint32_t source_age_ms;
    TimeConfidenceLevel confidence;
    bool authoritative;
} TimeSourceSnapshot;

/**
 * @brief Initialize the runtime time source state.
 *
 * Loads the persisted epoch baseline, classifies it as a recovered clock source,
 * and seeds the runtime snapshot used by UI, diagnostics, and web APIs.
 *
 * @return void
 * @throws None.
 */
void time_service_init(void);

/**
 * @brief Get the current epoch according to the active runtime source.
 *
 * The service advances the stored epoch baseline using monotonic uptime. This is
 * not equivalent to a hardware RTC on the ESP32-S3 profile.
 *
 * @return Current epoch in seconds since 1970-01-01 UTC.
 * @throws None.
 */
uint32_t time_service_get_epoch(void);

/**
 * @brief Convert the active epoch into a DateTime structure.
 *
 * @param[out] out Destination structure.
 * @return void
 * @throws None.
 * @boundary_behavior Returns immediately when @p out is NULL.
 */
void time_service_refresh(DateTime *out);

/**
 * @brief Set the active time from a DateTime using the host/manual source label.
 *
 * @param[in] dt Source date-time.
 * @return void
 * @throws None.
 * @boundary_behavior Invalid date-times are rejected silently to preserve legacy callers.
 */
void time_service_set_datetime(const DateTime *dt);

/**
 * @brief Validate and apply a DateTime from a specific source.
 *
 * @param[in] dt Source date-time.
 * @param[in] source Source classification for diagnostics and confidence tracking.
 * @return true when the input was accepted and persisted; false on invalid input.
 * @throws None.
 */
bool time_service_try_set_datetime(const DateTime *dt, TimeSourceType source);

/**
 * @brief Validate and apply an epoch from a specific source.
 *
 * @param[in] epoch Epoch to persist.
 * @param[in] source Source classification for diagnostics and confidence tracking.
 * @return true when the epoch was accepted and persisted; false when the epoch is outside the supported range.
 * @throws None.
 */
bool time_service_set_epoch_from_source(uint32_t epoch, TimeSourceType source);

/**
 * @brief Record a recovered/offline epoch state without asserting authoritative sync.
 *
 * @param[in] epoch Recovered epoch value.
 * @param[in] valid Whether the recovered epoch should be considered usable.
 * @return void
 * @throws None.
 * @boundary_behavior When @p valid is false, the runtime base clock is not modified.
 */
void time_service_note_recovery_epoch(uint32_t epoch, bool valid);

/**
 * @brief Copy the current time source snapshot.
 *
 * @param[out] out Destination snapshot.
 * @return true when @p out was populated; false when @p out is NULL.
 * @throws None.
 */
bool time_service_get_source_snapshot(TimeSourceSnapshot *out);
const char *time_service_source_name(TimeSourceType source);
const char *time_service_confidence_name(TimeConfidenceLevel confidence);
bool time_service_is_datetime_valid(const DateTime *dt);
uint32_t time_service_datetime_to_epoch(const DateTime *dt);
void time_service_epoch_to_datetime(uint32_t epoch, DateTime *out);
uint16_t time_service_day_id_from_epoch(uint32_t epoch);
bool time_service_is_reasonable_epoch(uint32_t epoch);

#ifdef __cplusplus
}
#endif

#endif

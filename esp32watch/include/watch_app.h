#ifndef WATCH_APP_H
#define WATCH_APP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WATCH_APP_STAGE_INPUT = 0,
    WATCH_APP_STAGE_SENSOR,
    WATCH_APP_STAGE_MODEL,
    WATCH_APP_STAGE_UI,
    WATCH_APP_STAGE_BATTERY,
    WATCH_APP_STAGE_ALERT,
    WATCH_APP_STAGE_DIAG,
    WATCH_APP_STAGE_STORAGE,
    WATCH_APP_STAGE_NETWORK,
    WATCH_APP_STAGE_WEB,
    WATCH_APP_STAGE_RENDER,
    WATCH_APP_STAGE_IDLE,
    WATCH_APP_STAGE_COUNT
} WatchAppStageId;

typedef enum {
    WATCH_APP_INIT_STAGE_NONE = 0,
    WATCH_APP_INIT_STAGE_DIAG,
    WATCH_APP_INIT_STAGE_POWER,
    WATCH_APP_INIT_STAGE_DISPLAY,
    WATCH_APP_INIT_STAGE_INPUT,
    WATCH_APP_INIT_STAGE_MODEL,
    WATCH_APP_INIT_STAGE_SENSOR,
    WATCH_APP_INIT_STAGE_ACTIVITY,
    WATCH_APP_INIT_STAGE_ALERT,
    WATCH_APP_INIT_STAGE_ALARM,
    WATCH_APP_INIT_STAGE_WDT,
    WATCH_APP_INIT_STAGE_BATTERY,
    WATCH_APP_INIT_STAGE_UI,
    WATCH_APP_INIT_STAGE_COUNT
} WatchAppInitStage;

typedef enum {
    WATCH_APP_INIT_STATUS_UNATTEMPTED = 0,
    WATCH_APP_INIT_STATUS_OK,
    WATCH_APP_INIT_STATUS_DEGRADED,
    WATCH_APP_INIT_STATUS_SKIPPED,
    WATCH_APP_INIT_STATUS_FAILED
} WatchAppInitStageStatus;

typedef struct {
    bool success;
    bool degraded;
    WatchAppInitStage failed_stage;
    WatchAppInitStage last_completed_stage;
    WatchAppInitStageStatus stage_status[WATCH_APP_INIT_STAGE_COUNT];
} WatchAppInitReport;

typedef struct {
    uint32_t budget_ms;
    uint32_t last_duration_ms;
    uint32_t max_duration_ms;
    uint32_t total_duration_ms;
    uint32_t sample_count;
    uint16_t over_budget_count;
    uint16_t consecutive_over_budget;
    uint16_t max_consecutive_over_budget;
    uint16_t deferred_count;
} WatchAppStageTelemetry;

typedef struct {
    uint32_t timestamp_ms;
    uint16_t duration_ms;
    uint16_t budget_ms;
    uint32_t loop_counter;
    uint8_t over_budget;
    uint8_t deferred;
    WatchAppStageId stage;
} WatchAppStageHistoryEntry;

void watch_app_init(void);

/**
 * @brief Initialize the watch application and expose per-stage bring-up status.
 *
 * @param[out] out Optional destination for the initialization report.
 * @return true when every required runtime stage completed; false when a required stage failed.
 * @throws None.
 * @boundary_behavior Returns false and leaves the application runtime report updated when a required stage fails; optional/degraded stages are recorded in @p out when provided.
 */
bool watch_app_init_checked(WatchAppInitReport *out);

/**
 * @brief Retrieve the most recent watch application initialization report.
 *
 * @param[out] out Destination for the retained initialization report.
 * @return true when @p out was populated; false when @p out is NULL.
 * @throws None.
 */
bool watch_app_get_init_report(WatchAppInitReport *out);

/**
 * @brief Map an initialization stage enum to a stable diagnostic name.
 *
 * @param[in] stage Initialization stage identifier.
 * @return Constant stage name string, or "UNKNOWN" when @p stage is out of range.
 * @throws None.
 */
const char *watch_app_init_stage_name(WatchAppInitStage stage);

/**
 * @brief Map an initialization status enum to a stable diagnostic name.
 *
 * @param[in] status Initialization status identifier.
 * @return Constant status name string, or "UNKNOWN" when @p status is out of range.
 * @throws None.
 */
const char *watch_app_init_status_name(WatchAppInitStageStatus status);
void watch_app_task(void);
void watch_app_request_render(void);
const char *watch_app_stage_name(WatchAppStageId id);
bool watch_app_get_stage_telemetry(WatchAppStageId id, WatchAppStageTelemetry *out);
uint8_t watch_app_get_stage_history_count(void);
bool watch_app_get_stage_history_recent(uint8_t reverse_index, WatchAppStageHistoryEntry *out);

#endif

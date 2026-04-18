#ifndef WDT_SERVICE_H
#define WDT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WDT_CHECKPOINT_NONE = 0,
    WDT_CHECKPOINT_INPUT,
    WDT_CHECKPOINT_SENSOR,
    WDT_CHECKPOINT_MODEL,
    WDT_CHECKPOINT_UI,
    WDT_CHECKPOINT_BATTERY,
    WDT_CHECKPOINT_ALERT,
    WDT_CHECKPOINT_DIAG,
    WDT_CHECKPOINT_STORAGE,
    WDT_CHECKPOINT_NETWORK,
    WDT_CHECKPOINT_WEB,
    WDT_CHECKPOINT_RENDER,
    WDT_CHECKPOINT_IDLE
} WdtCheckpoint;

typedef enum {
    WDT_CHECKPOINT_RESULT_OK = 0,
    WDT_CHECKPOINT_RESULT_DEGRADED,
    WDT_CHECKPOINT_RESULT_FAILED
} WdtCheckpointResult;

void wdt_service_init(void);
void wdt_service_begin_loop(uint32_t now_ms);
void wdt_service_note_checkpoint(WdtCheckpoint checkpoint, uint32_t now_ms);
void wdt_service_note_checkpoint_result(WdtCheckpoint checkpoint, WdtCheckpointResult result, uint32_t now_ms);
void wdt_service_tick(uint32_t now_ms, bool allow_refresh);
uint32_t wdt_service_get_loop_count(void);
uint32_t wdt_service_get_max_loop_ms(void);
WdtCheckpoint wdt_service_get_last_checkpoint(void);
WdtCheckpointResult wdt_service_get_last_checkpoint_result(void);
const char *wdt_service_checkpoint_name(WdtCheckpoint checkpoint);
const char *wdt_service_checkpoint_result_name(WdtCheckpointResult result);

bool wdt_service_is_initialized(void);

#endif

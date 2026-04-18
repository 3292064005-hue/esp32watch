#include "services/wdt_service.h"
#include "board_features.h"
#include "crash_capsule.h"
#include "services/diag_service.h"
#include "system_init.h"

#define WDT_REQUIRED_MASK ((1UL << WDT_CHECKPOINT_INPUT) |                            (1UL << WDT_CHECKPOINT_SENSOR) |                            (1UL << WDT_CHECKPOINT_MODEL) |                            (1UL << WDT_CHECKPOINT_UI) |                            (1UL << WDT_CHECKPOINT_BATTERY) |                            (1UL << WDT_CHECKPOINT_ALERT) |                            (1UL << WDT_CHECKPOINT_DIAG) |                            (1UL << WDT_CHECKPOINT_STORAGE) |                            (1UL << WDT_CHECKPOINT_NETWORK) |                            (1UL << WDT_CHECKPOINT_WEB))

typedef struct {
    uint32_t loop_started_ms;
    uint32_t loop_count;
    uint32_t max_loop_ms;
    uint32_t checkpoint_mask;
    WdtCheckpoint last_checkpoint;
    WdtCheckpointResult last_checkpoint_result;
    bool loop_active;
} WdtServiceState;

static WdtServiceState g_wdt;
static bool g_wdt_initialized;

static bool wdt_service_hw_ready(void)
{
#if APP_FEATURE_WATCHDOG
    return system_runtime_watchdog_available() &&
           system_init_stage_completed(SYSTEM_INIT_STAGE_IWDG);
#else
    return false;
#endif
}

void wdt_service_init(void)
{
    g_wdt_initialized = true;
    g_wdt.loop_started_ms = 0U;
    g_wdt.loop_count = 0U;
    g_wdt.max_loop_ms = 0U;
    g_wdt.checkpoint_mask = 0U;
    g_wdt.last_checkpoint = WDT_CHECKPOINT_NONE;
    g_wdt.last_checkpoint_result = WDT_CHECKPOINT_RESULT_OK;
    g_wdt.loop_active = false;
#if APP_FEATURE_WATCHDOG
    if (wdt_service_hw_ready()) {
        diag_service_note_wdt_init();
    }
#endif
}

void wdt_service_begin_loop(uint32_t now_ms)
{
    g_wdt.loop_started_ms = now_ms;
    g_wdt.checkpoint_mask = 0U;
    g_wdt.last_checkpoint = WDT_CHECKPOINT_NONE;
    g_wdt.last_checkpoint_result = WDT_CHECKPOINT_RESULT_OK;
    g_wdt.loop_active = true;
    crash_capsule_note_loop_start(now_ms);
}

void wdt_service_note_checkpoint_result(WdtCheckpoint checkpoint, WdtCheckpointResult result, uint32_t now_ms)
{
    if (!g_wdt.loop_active || checkpoint <= WDT_CHECKPOINT_NONE || checkpoint > WDT_CHECKPOINT_IDLE) {
        return;
    }
    g_wdt.last_checkpoint = checkpoint;
    g_wdt.last_checkpoint_result = result;
    g_wdt.checkpoint_mask |= (1UL << checkpoint);
    if (now_ms >= g_wdt.loop_started_ms) {
        uint32_t loop_ms = now_ms - g_wdt.loop_started_ms;
        if (loop_ms > g_wdt.max_loop_ms) {
            g_wdt.max_loop_ms = loop_ms;
        }
    }
    crash_capsule_note_checkpoint_result((uint8_t)checkpoint, (uint8_t)result, now_ms);
}

void wdt_service_note_checkpoint(WdtCheckpoint checkpoint, uint32_t now_ms)
{
    wdt_service_note_checkpoint_result(checkpoint, WDT_CHECKPOINT_RESULT_OK, now_ms);
}

void wdt_service_tick(uint32_t now_ms, bool allow_refresh)
{
    uint32_t loop_ms = 0U;
    if (g_wdt.loop_active && now_ms >= g_wdt.loop_started_ms) {
        loop_ms = now_ms - g_wdt.loop_started_ms;
        if (loop_ms > g_wdt.max_loop_ms) {
            g_wdt.max_loop_ms = loop_ms;
        }
    }

#if APP_FEATURE_WATCHDOG
    if (!wdt_service_hw_ready()) {
        if (allow_refresh && g_wdt.loop_active && (g_wdt.checkpoint_mask & WDT_REQUIRED_MASK) == WDT_REQUIRED_MASK) {
            g_wdt.loop_count++;
            g_wdt.loop_active = false;
        }
        return;
    }
    if (g_wdt.loop_active && loop_ms > 900U && (g_wdt.checkpoint_mask & WDT_REQUIRED_MASK) != WDT_REQUIRED_MASK) {
        diag_service_note_checkpoint_timeout((uint8_t)g_wdt.last_checkpoint,
                                             (uint16_t)(loop_ms > 0xFFFFU ? 0xFFFFU : loop_ms));
    }
    if (allow_refresh && g_wdt.loop_active && (g_wdt.checkpoint_mask & WDT_REQUIRED_MASK) == WDT_REQUIRED_MASK) {
        (void)platform_watchdog_kick(&platform_watchdog_main);
        diag_service_note_wdt_pet();
        g_wdt.loop_count++;
        g_wdt.loop_active = false;
    }
#else
    if (allow_refresh && g_wdt.loop_active && (g_wdt.checkpoint_mask & WDT_REQUIRED_MASK) == WDT_REQUIRED_MASK) {
        g_wdt.loop_count++;
        g_wdt.loop_active = false;
    }
#endif
}

uint32_t wdt_service_get_loop_count(void)
{
    return g_wdt.loop_count;
}

uint32_t wdt_service_get_max_loop_ms(void)
{
    return g_wdt.max_loop_ms;
}

WdtCheckpoint wdt_service_get_last_checkpoint(void)
{
    return g_wdt.last_checkpoint;
}

WdtCheckpointResult wdt_service_get_last_checkpoint_result(void)
{
    return g_wdt.last_checkpoint_result;
}

const char *wdt_service_checkpoint_name(WdtCheckpoint checkpoint)
{
    switch (checkpoint) {
        case WDT_CHECKPOINT_INPUT: return "IN";
        case WDT_CHECKPOINT_SENSOR: return "SENS";
        case WDT_CHECKPOINT_MODEL: return "MODEL";
        case WDT_CHECKPOINT_UI: return "UI";
        case WDT_CHECKPOINT_BATTERY: return "BATT";
        case WDT_CHECKPOINT_ALERT: return "ALERT";
        case WDT_CHECKPOINT_DIAG: return "DIAG";
        case WDT_CHECKPOINT_STORAGE: return "STORE";
        case WDT_CHECKPOINT_NETWORK: return "NET";
        case WDT_CHECKPOINT_WEB: return "WEB";
        case WDT_CHECKPOINT_RENDER: return "REND";
        case WDT_CHECKPOINT_IDLE: return "IDLE";
        default: return "NONE";
    }
}

const char *wdt_service_checkpoint_result_name(WdtCheckpointResult result)
{
    switch (result) {
        case WDT_CHECKPOINT_RESULT_DEGRADED: return "DEGR";
        case WDT_CHECKPOINT_RESULT_FAILED: return "FAIL";
        default: return "OK";
    }
}

bool wdt_service_is_initialized(void)
{
    return g_wdt_initialized;
}

#include "watch_app.h"
#include "watch_app_internal.h"
#include "ui.h"
#include <stddef.h>

static WatchAppRuntimeState g_watch_app_runtime;

void watch_app_init(void)
{
    (void)watch_app_init_checked(NULL);
}

bool watch_app_init_checked(WatchAppInitReport *out)
{
    return watch_app_runtime_init_state_checked(&g_watch_app_runtime, out);
}

bool watch_app_get_init_report(WatchAppInitReport *out)
{
    if (out == NULL) {
        return false;
    }
    *out = g_watch_app_runtime.init_report;
    return true;
}


void watch_app_task(void)
{
    watch_app_runtime_run_loop(&g_watch_app_runtime);
}

void watch_app_request_render(void)
{
    ui_request_render();
}

const char *watch_app_stage_name(WatchAppStageId id)
{
    return watch_app_runtime_stage_name(id);
}

bool watch_app_get_stage_telemetry(WatchAppStageId id, WatchAppStageTelemetry *out)
{
    return watch_app_runtime_get_stage_telemetry(&g_watch_app_runtime, id, out);
}

uint8_t watch_app_get_stage_history_count(void)
{
    return watch_app_runtime_get_stage_history_count(&g_watch_app_runtime);
}

bool watch_app_get_stage_history_recent(uint8_t reverse_index, WatchAppStageHistoryEntry *out)
{
    return watch_app_runtime_get_stage_history_recent(&g_watch_app_runtime, reverse_index, out);
}

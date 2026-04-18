#include "watch_app_internal.h"
#include "ui_internal.h"
#include "services/storage_service.h"
#include "services/wdt_service.h"
#include "services/diag_service.h"
#include "services/network_sync_service.h"
#include "web/web_server.h"
#include "web/web_wifi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool ui_get_runtime_snapshot(UiRuntimeSnapshot *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->current = PAGE_WATCHFACE;
    }
    return true;
}

const ModelUiState *model_get_ui_state(ModelUiState *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return out;
}

const UiPageOps *ui_page_registry_get(PageId page)
{
    static const UiPageOps ops = {
        .title = "WATCH",
        .refresh_policy = UI_REFRESH_NONE,
        .allow_auto_sleep = true,
        .allow_popup = true,
        .nav_group = UI_NAV_GROUP_ROOT,
        .presenter = UI_PRESENTER_WATCHFACE,
        .render = NULL,
        .handle = NULL,
    };
    (void)page;
    return &ops;
}

static bool g_storage_pending = false;
static bool g_storage_transaction_active = false;
static bool g_storage_sleep_flush_pending = false;
static bool g_diag_safe_mode_active = false;
static bool g_network_snapshot_available = true;
static bool g_wifi_connected = true;
static bool g_time_synced = true;
static bool g_weather_last_attempt_ok = true;
static int32_t g_last_weather_http_status = 200;
static bool g_web_server_ready = true;
static bool g_web_filesystem_ready = true;
static bool g_web_assets_ready = true;
static bool g_web_network_route = true;

bool storage_service_has_pending(void) { return g_storage_pending; }
bool storage_service_is_transaction_active(void) { return g_storage_transaction_active; }
bool storage_service_is_sleep_flush_pending(void) { return g_storage_sleep_flush_pending; }
StorageCommitState storage_service_get_commit_state(void) { return STORAGE_COMMIT_STATE_IDLE; }
uint16_t input_service_get_overflow_count(void) { return 0U; }
bool diag_service_is_safe_mode_active(void) { return g_diag_safe_mode_active; }
const ModelRuntimeState *model_get_runtime_state(ModelRuntimeState *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->storage_ok = true;
        out->storage_crc_ok = true;
    }
    return out;
}
const ModelDomainState *model_get_domain_state(ModelDomainState *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->time_valid = true;
        out->alarm_ringing_index = 0xFFU;
    }
    return out;
}
bool diag_service_get_last_fault(DiagFaultCode *code, DiagFaultInfo *info)
{
    if (code != NULL) {
        *code = DIAG_FAULT_NONE;
    }
    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
    return false;
}

bool network_sync_service_get_snapshot(NetworkSyncSnapshot *out)
{
    if (!g_network_snapshot_available) {
        return false;
    }
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->wifi_connected = g_wifi_connected;
        out->time_synced = g_time_synced;
        out->weather_last_attempt_ok = g_weather_last_attempt_ok;
        out->last_weather_http_status = g_last_weather_http_status;
    }
    return true;
}

bool web_server_is_ready(void) { return g_web_server_ready; }
bool web_server_filesystem_ready(void) { return g_web_filesystem_ready; }
bool web_server_filesystem_assets_ready(void) { return g_web_assets_ready; }
bool web_wifi_has_network_route(void) { return g_web_network_route; }

uint32_t watch_app_stage_clock_now(void)
{
    return 0U;
}

int main(void)
{
    WatchAppStageTelemetry telemetry[WATCH_APP_STAGE_COUNT];
    WatchAppStageHistoryEntry history[WATCH_APP_STAGE_HISTORY_CAPACITY];
    uint8_t head = 0U;
    uint8_t count = 0U;

    watch_app_reset_stage_telemetry(telemetry, history, &head, &count);
    assert(telemetry[WATCH_APP_STAGE_NETWORK].budget_ms > 0U);
    watch_app_record_stage_timing(WATCH_APP_STAGE_WEB, 10U, 18U, telemetry, history, &head, &count, 7U);
    assert(telemetry[WATCH_APP_STAGE_WEB].last_duration_ms == 8U);
    assert(telemetry[WATCH_APP_STAGE_WEB].sample_count == 1U);
    watch_app_note_stage_deferred(WATCH_APP_STAGE_NETWORK, telemetry, history, &head, &count, 8U);
    assert(telemetry[WATCH_APP_STAGE_NETWORK].deferred_count == 1U);
    assert(count >= 1U);
    assert(watch_app_should_run_diag_stage(telemetry, 8U));
    assert(watch_app_result_storage_stage() == WDT_CHECKPOINT_RESULT_OK);
    assert(watch_app_result_network_stage() == WDT_CHECKPOINT_RESULT_OK);
    assert(watch_app_result_web_stage() == WDT_CHECKPOINT_RESULT_OK);

    g_storage_pending = true;
    assert(watch_app_result_storage_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_storage_pending = false;
    g_storage_transaction_active = true;
    assert(watch_app_result_storage_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_storage_transaction_active = false;

    g_network_snapshot_available = false;
    assert(watch_app_result_network_stage() == WDT_CHECKPOINT_RESULT_FAILED);
    g_network_snapshot_available = true;
    g_web_network_route = false;
    assert(watch_app_result_network_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_web_network_route = true;
    g_wifi_connected = false;
    assert(watch_app_result_network_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_wifi_connected = true;
    g_time_synced = false;
    assert(watch_app_result_network_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_time_synced = true;
    g_weather_last_attempt_ok = false;
    g_last_weather_http_status = -1;
    assert(watch_app_result_network_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_weather_last_attempt_ok = true;
    g_last_weather_http_status = 200;

    g_web_filesystem_ready = false;
    assert(watch_app_result_web_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_web_filesystem_ready = true;
    g_web_assets_ready = false;
    assert(watch_app_result_web_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_web_assets_ready = true;
    g_web_server_ready = false;
    assert(watch_app_result_web_stage() == WDT_CHECKPOINT_RESULT_DEGRADED);
    g_web_server_ready = true;

    puts("[OK] watch_app_policy behavior check passed");
    return 0;
}

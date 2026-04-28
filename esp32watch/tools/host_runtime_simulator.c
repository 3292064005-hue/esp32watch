#include "watch_app_internal.h"
#include "ui_internal.h"
#include "ui_page_registry.h"
#include "display_internal.h"
#include "model.h"
#include "sensor.h"
#include "services/runtime_event_service.h"
#include "services/network_sync_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include "services/wdt_service.h"
#include "services/time_service.h"
#include "services/power_service.h"
#include "power_policy.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ModelDomainState g_domain;
static ModelRuntimeState g_runtime;
static ModelUiState g_model_ui;
static UiSystemSnapshot g_snap;
static UiRuntimeSnapshot g_ui_runtime;
static SensorSnapshot g_sensor_snapshot;
static NetworkSyncSnapshot g_network_snapshot;
static uint32_t g_now_ms;
static uint32_t g_stage_clock_ms;
static uint32_t g_loop_count;
static PageId g_requested_page = PAGE_WATCHFACE;

static uint32_t fnv1a32(const uint8_t *data, size_t len)
{
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 16777619UL;
    }
    return h;
}

static uint32_t count_lit_pixels(void)
{
    uint32_t lit_pixels = 0U;
    for (size_t j = 0; j < sizeof(g_oled_buffer); ++j) {
        uint8_t byte = g_oled_buffer[j];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            lit_pixels += (byte >> bit) & 0x1U;
        }
    }
    return lit_pixels;
}

bool display_backend_init(void) { return true; }
void display_backend_set_contrast(uint8_t value) { (void)value; }
void display_backend_set_sleep(bool sleep) { (void)sleep; }
bool display_backend_flush(const uint8_t *buffer) { (void)buffer; return true; }
void diag_service_note_display_tx_fail(uint16_t code) { (void)code; }

uint32_t platform_time_now_ms(void) { return g_now_ms; }
uint32_t watch_app_stage_clock_now(void) { return ++g_stage_clock_ms; }
void delay(uint32_t ms) { g_now_ms += ms; }

const ModelDomainState *model_get_domain_state(ModelDomainState *out) { if (!out) return NULL; *out = g_domain; return out; }
const ModelRuntimeState *model_get_runtime_state(ModelRuntimeState *out) { if (!out) return NULL; *out = g_runtime; return out; }
const ModelUiState *model_get_ui_state(ModelUiState *out) { if (!out) return NULL; *out = g_model_ui; return out; }
uint32_t model_consume_runtime_requests(StorageCommitReason *reason) { if (reason) *reason = STORAGE_COMMIT_REASON_NONE; return 0U; }
void model_sync_runtime_observability(void) {}
void model_flush_read_snapshots(void) {}
void model_tick(uint32_t now_ms) { (void)now_ms; }

bool network_sync_service_get_snapshot(NetworkSyncSnapshot *out) { if (!out) return false; *out = g_network_snapshot; return true; }
void network_sync_service_tick(uint32_t now_ms) { (void)now_ms; }
uint32_t network_sync_service_last_applied_generation(void) { return 1U; }
bool network_sync_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg) { (void)cfg; return generation == 1U; }

bool time_service_get_source_snapshot(TimeSourceSnapshot *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->source = TIME_SOURCE_NETWORK_SYNC;
    out->confidence = TIME_CONFIDENCE_HIGH;
    return true;
}
const char *time_service_source_name(TimeSourceType type) { return type == TIME_SOURCE_NETWORK_SYNC ? "NTP" : "NONE"; }
const char *time_service_confidence_name(TimeConfidenceLevel level) { return level == TIME_CONFIDENCE_HIGH ? "HIGH" : "LOW"; }

void input_service_tick(void) {}
uint16_t input_service_get_overflow_count(void) { return 0U; }

void sensor_service_tick(uint32_t now_ms) { g_sensor_snapshot.last_sample_ms = now_ms; }
const SensorSnapshot *sensor_service_get_snapshot(void) { return &g_sensor_snapshot; }
void sensor_service_set_sensitivity(uint8_t sensitivity) { g_domain.settings.sensor_sensitivity = sensitivity; }
void sensor_service_clear_calibration(void) { g_sensor_snapshot.calibrated = false; }
void sensor_service_note_storage_commit_result(StorageCommitReason reason, bool ok) { (void)reason; (void)ok; }

void activity_service_ingest_snapshot(const SensorSnapshot *snap, uint32_t now_ms) { (void)snap; (void)now_ms; }
void alarm_service_tick(uint32_t now_ms) { (void)now_ms; }
void battery_service_tick(uint32_t now_ms) { (void)now_ms; }
void alert_service_tick(uint32_t now_ms) { (void)now_ms; }

bool storage_service_has_pending(void) { return false; }
bool storage_service_is_transaction_active(void) { return false; }
bool storage_service_is_sleep_flush_pending(void) { return false; }
void storage_service_tick(uint32_t now_ms) { (void)now_ms; }
uint32_t storage_service_get_last_commit_ms(void) { return 0U; }
StorageCommitReason storage_service_get_last_commit_reason(void) { return STORAGE_COMMIT_REASON_NONE; }
bool storage_service_get_last_commit_ok(void) { return true; }
StorageCommitState storage_service_get_commit_state(void) { return STORAGE_COMMIT_STATE_IDLE; }
void storage_service_request_flush_before_sleep(void) {}
void storage_service_request_commit(StorageCommitReason reason) { (void)reason; }

void wdt_service_begin_loop(uint32_t now_ms) { (void)now_ms; }
void wdt_service_note_checkpoint_result(WdtCheckpoint checkpoint, WdtCheckpointResult result, uint32_t now_ms) { (void)checkpoint; (void)result; (void)now_ms; }
void wdt_service_tick(uint32_t now_ms, bool main_loop_healthy) { (void)now_ms; (void)main_loop_healthy; }

bool diag_service_is_safe_mode_active(void) { return false; }
bool diag_service_can_clear_safe_mode(void) { return true; }
DiagSafeModeReason diag_service_get_safe_mode_reason(void) { return DIAG_SAFE_MODE_NONE; }
const char *diag_service_safe_mode_reason_name(DiagSafeModeReason reason) { (void)reason; return "NONE"; }
bool diag_service_get_last_fault(DiagFaultCode *code, DiagFaultInfo *out) { (void)code; (void)out; return false; }
void diag_service_tick(uint32_t now_ms) { (void)now_ms; }

bool web_server_has_pending_actions(void) { return false; }
bool web_server_filesystem_ready(void) { return true; }
bool web_server_filesystem_assets_ready(void) { return true; }
bool web_server_is_ready(void) { return true; }
void web_server_poll(uint32_t now_ms) { (void)now_ms; }

bool web_overlay_render_if_active(uint32_t now_ms) { (void)now_ms; return false; }

bool web_wifi_connected(void) { return true; }
bool web_wifi_transition_active(void) { return false; }
bool web_wifi_access_point_active(void) { return false; }
const char *web_wifi_access_point_ssid(void) { return ""; }
const char *web_wifi_access_point_password(void) { return ""; }
bool web_wifi_serial_password_log_enabled(void) { return false; }
const char *web_wifi_mode_name(void) { return "STA"; }
const char *web_wifi_status_name(void) { return "CONNECTED"; }
bool web_wifi_has_network_route(void) { return true; }
uint32_t web_wifi_last_applied_generation(void) { return 1U; }
bool web_wifi_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg) { (void)cfg; return generation == 1U; }
const char *web_wifi_ip_string(void) { return "192.168.1.10"; }
int32_t web_wifi_rssi(void) { return -48; }

void power_policy_reset_qos_registry(void) {}
bool power_policy_register_qos_provider(PowerQosProviderFn provider, void *ctx) { (void)provider; (void)ctx; return true; }
void power_policy_collect_qos_snapshot(PowerQosSnapshot *out) { if (out) memset(out, 0, sizeof(*out)); }
bool power_policy_can_enter_cpu_idle_ex(const PowerQosSnapshot *snapshot, uint32_t *suggested_idle_ms) { (void)snapshot; if (suggested_idle_ms) *suggested_idle_ms = 0U; return false; }
bool power_policy_platform_pm_lock_enter(uint32_t qos_mask, const char *owner) { (void)qos_mask; (void)owner; return true; }
bool power_policy_platform_pm_lock_exit(uint32_t qos_mask, const char *owner) { (void)qos_mask; (void)owner; return true; }

void power_service_idle(bool can_idle_sleep) { (void)can_idle_sleep; }
void power_service_request_sleep(SleepReason reason) { (void)reason; }
void power_service_set_sleeping(bool sleeping) { g_ui_runtime.sleeping = sleeping; }

const char *ui_weekday_name(uint8_t weekday) {
    static const char *names[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    return names[weekday % 7U];
}
void ui_get_system_snapshot(UiSystemSnapshot *out) { if (out) *out = g_snap; }
void ui_status_compose_header_tags(char *out, size_t out_size) { if (out && out_size) { strncpy(out, "SYNC", out_size - 1U); out[out_size - 1U] = '\0'; } }
void ui_core_draw_status_bar(int16_t ox) { display_draw_rect(ox, 0, 128, 10, true); }
void ui_core_draw_header(int16_t ox, const char *title) { display_draw_rect(ox, 0, 128, 12, true); display_draw_text_centered_5x7(ox, 2, 128, title, true); }
void ui_core_draw_card(int16_t x, int16_t y, int16_t w, int16_t h, const char *title) { display_draw_round_rect(x, y, w, h, true); display_draw_text_centered_5x7(x, y + 2, w, title, true); }
void ui_core_draw_kv_row(int16_t x, int16_t y, int16_t w, const char *key, const char *value) { display_draw_text_5x7(x, y, key, true); display_draw_text_right_5x7(x + w, y, value, true); }
void ui_core_draw_footer_hint(int16_t ox, const char *hint) { display_draw_hline(ox, 55, 128, true); display_draw_text_centered_5x7(ox, 57, 128, hint, true); }
void ui_core_mark_dirty(void) { g_ui_runtime.dirty = true; }
void ui_runtime_set_sleeping(bool sleeping) { g_ui_runtime.sleeping = sleeping; }

bool ui_get_runtime_snapshot(UiRuntimeSnapshot *out) { if (!out) return false; *out = g_ui_runtime; return true; }
uint32_t ui_consume_actions(void) { return UI_ACTION_NONE; }
bool ui_consume_model_mutation(UiModelMutation *out) { (void)out; return false; }
void ui_request_storage_manual_flush(void) {}
void ui_request_cycle_watchface(int8_t dir) { (void)dir; }
void ui_core_go(PageId page, int8_t dir, uint32_t now_ms) { (void)dir; (void)now_ms; g_requested_page = page; g_ui_runtime.current = page; g_ui_runtime.dirty = true; }
void ui_request_sensor_reinit(void) {}
void ui_request_sensor_calibration(void) {}
void ui_request_clear_safe_mode(void) {}
void ui_tick(uint32_t now_ms) { (void)now_ms; g_ui_runtime.current = g_requested_page; g_ui_runtime.last_input_ms = now_ms; }
bool ui_has_pending_actions(void) { return false; }
bool ui_should_render(uint32_t now_ms) { (void)now_ms; return g_ui_runtime.dirty || g_ui_runtime.last_render_ms == 0U; }
void ui_render(void)
{
    display_clear();
    switch (g_ui_runtime.current) {
        case PAGE_WATCHFACE: ui_page_watchface_render(g_ui_runtime.current, 0); break;
        case PAGE_ACTIVITY: ui_page_activity_render(g_ui_runtime.current, 0); break;
        case PAGE_STORAGE: ui_page_storage_render(g_ui_runtime.current, 0); break;
        case PAGE_ABOUT: ui_page_about_render(g_ui_runtime.current, 0); break;
        default: ui_page_watchface_render(PAGE_WATCHFACE, 0); break;
    }
    g_ui_runtime.last_render_ms = g_now_ms;
    g_ui_runtime.dirty = false;
}

const UiPageOps *ui_page_registry_get(PageId page)
{
    static UiPageOps ops;
    memset(&ops, 0, sizeof(ops));
    ops.refresh_policy = (page == PAGE_WATCHFACE) ? UI_REFRESH_WATCHFACE : UI_REFRESH_CARD;
    return &ops;
}
const char *ui_page_registry_title(PageId page) { (void)page; return "PAGE"; }
UiPageRefreshPolicy ui_page_registry_refresh_policy(PageId page) { return ui_page_registry_get(page)->refresh_policy; }
bool ui_page_registry_allows_auto_sleep(PageId page) { (void)page; return true; }
bool ui_page_registry_allows_popup(PageId page) { (void)page; return true; }
uint8_t ui_page_registry_nav_group(PageId page) { (void)page; return 0U; }
UiPagePresenterId ui_page_registry_presenter(PageId page) { (void)page; return UI_PRESENTER_NONE; }

void watch_app_apply_model_runtime_requests(uint8_t *last_sensor_sensitivity) { (void)last_sensor_sensitivity; }
void watch_app_apply_ui_actions(WatchAppSleepRequestState *sleep_request, uint8_t *last_sensor_sensitivity) { (void)sleep_request; (void)last_sensor_sensitivity; }
void watch_app_after_storage_tick(WatchAppSleepRequestState *sleep_request, uint32_t *last_storage_commit_ms) { (void)sleep_request; (void)last_storage_commit_ms; }

static void seed_state(void)
{
    memset(&g_domain, 0, sizeof(g_domain));
    memset(&g_runtime, 0, sizeof(g_runtime));
    memset(&g_model_ui, 0, sizeof(g_model_ui));
    memset(&g_snap, 0, sizeof(g_snap));
    memset(&g_ui_runtime, 0, sizeof(g_ui_runtime));
    memset(&g_sensor_snapshot, 0, sizeof(g_sensor_snapshot));
    memset(&g_network_snapshot, 0, sizeof(g_network_snapshot));

    g_domain.time_valid = true;
    g_domain.now.year = 2026; g_domain.now.month = 4; g_domain.now.day = 13; g_domain.now.weekday = 1;
    g_domain.now.hour = 9; g_domain.now.minute = 41; g_domain.now.second = 7;
    g_domain.settings.watchface = 0U;
    g_domain.settings.show_seconds = true;
    g_domain.settings.sensor_sensitivity = 2U;
    g_domain.activity.goal = 8000U;
    g_domain.activity.steps = 3456U;
    g_runtime.sensor.online = true;
    g_runtime.sensor.quality = 80U;
    g_runtime.storage_ok = true;
    g_runtime.storage_crc_ok = true;
    g_runtime.reset_reason = RESET_REASON_POWER_ON;
    g_model_ui.popup = POPUP_NONE;

    g_snap.activity.steps = 3456U;
    g_snap.activity.goal = 8000U;
    g_snap.activity.goal_percent = 43U;
    g_snap.activity.active_minutes = 27U;
    g_snap.storage.version = 6U;
    g_snap.storage.crc_valid = true;
    g_snap.storage.pending_mask = 0x00U;
    g_snap.storage.dirty_source_mask = 0x00U;
    g_snap.storage.commit_count = 7U;
    g_snap.storage.wear_count = 9U;
    g_snap.storage.backend_name = "PREFERENCES";

    g_sensor_snapshot.online = true;
    g_sensor_snapshot.calibrated = true;
    g_sensor_snapshot.quality = 80U;
    g_sensor_snapshot.runtime_state = SENSOR_STATE_READY;

    g_network_snapshot.wifi_connected = true;
    g_network_snapshot.time_synced = true;
    strncpy(g_network_snapshot.weather_status, "SYNCED", sizeof(g_network_snapshot.weather_status) - 1U);
    strncpy(g_network_snapshot.location_name, "Lab", sizeof(g_network_snapshot.location_name) - 1U);

    g_ui_runtime.current = PAGE_WATCHFACE;
    g_ui_runtime.dirty = true;
    g_requested_page = PAGE_WATCHFACE;
}

static const char *page_name(PageId page)
{
    switch (page) {
        case PAGE_WATCHFACE: return "WATCHFACE";
        case PAGE_ACTIVITY: return "ACTIVITY";
        case PAGE_STORAGE: return "STORAGE";
        case PAGE_ABOUT: return "ABOUT";
        default: return "UNKNOWN";
    }
}

static bool parse_page(const char *name, PageId *out)
{
    if (name == NULL || out == NULL) {
        return false;
    }
    if (strcmp(name, "WATCHFACE") == 0) { *out = PAGE_WATCHFACE; return true; }
    if (strcmp(name, "ACTIVITY") == 0) { *out = PAGE_ACTIVITY; return true; }
    if (strcmp(name, "STORAGE") == 0) { *out = PAGE_STORAGE; return true; }
    if (strcmp(name, "ABOUT") == 0) { *out = PAGE_ABOUT; return true; }
    return false;
}

int main(int argc, char **argv)
{
    const char *output_path = NULL;
    struct Step { PageId page; uint32_t now_ms; } steps[32];
    size_t step_count = 0U;
    WatchAppRuntimeState state = {0};

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--output") == 0 && (i + 1) < argc) {
            output_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--step") == 0 && (i + 1) < argc) {
            const char *spec = argv[++i];
            const char *colon = strchr(spec, ':');
            char page_buf[32];
            unsigned long now_ms;
            PageId page;
            if (colon == NULL || step_count >= (sizeof(steps) / sizeof(steps[0]))) {
                fprintf(stderr, "invalid --step %s\n", spec);
                return 1;
            }
            size_t page_len = (size_t)(colon - spec);
            if (page_len == 0U || page_len >= sizeof(page_buf)) {
                fprintf(stderr, "invalid page spec %s\n", spec);
                return 1;
            }
            memcpy(page_buf, spec, page_len);
            page_buf[page_len] = '\0';
            if (!parse_page(page_buf, &page)) {
                fprintf(stderr, "unsupported page %s\n", page_buf);
                return 1;
            }
            now_ms = strtoul(colon + 1, NULL, 10);
            steps[step_count].page = page;
            steps[step_count].now_ms = (uint32_t)now_ms;
            ++step_count;
            continue;
        }
        fprintf(stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    if (output_path == NULL || step_count == 0U) {
        fprintf(stderr, "usage: host_runtime_simulator --step PAGE:MS [...] --output path\n");
        return 1;
    }

    seed_state();
    display_init();
    watch_app_reset_stage_telemetry(state.stage_telemetry,
                                    state.stage_history,
                                    &state.stage_history_head,
                                    &state.stage_history_count);
    watch_app_register_qos_providers(&state);
    state.last_sensor_sensitivity = g_domain.settings.sensor_sensitivity;

    FILE *fp = fopen(output_path, "w");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    fprintf(fp, "{\n  \"simulator\": \"host_runtime_simulator\",\n  \"schemaVersion\": 2,\n  \"frames\": [\n");
    for (size_t i = 0; i < step_count; ++i) {
        g_requested_page = steps[i].page;
        g_ui_runtime.current = steps[i].page;
        g_ui_runtime.dirty = true;
        g_now_ms = steps[i].now_ms;
        watch_app_runtime_run_loop(&state);
        g_loop_count = state.loop_counter;
        fprintf(fp,
                "    {\"index\": %zu, \"page\": \"%s\", \"nowMs\": %u, \"frameCrc32\": %u, \"litPixels\": %u, \"loopCount\": %u, \"stageHistoryCount\": %u}%s\n",
                i,
                page_name(steps[i].page),
                steps[i].now_ms,
                fnv1a32(g_oled_buffer, sizeof(g_oled_buffer)),
                count_lit_pixels(),
                state.loop_counter,
                (unsigned)watch_app_runtime_get_stage_history_count(&state),
                (i + 1U) == step_count ? "" : ",");
    }
    fprintf(fp, "  ],\n  \"runtime\": {\n    \"finalLoopCount\": %u,\n    \"stageHistoryCount\": %u,\n    \"currentPage\": \"%s\"\n  }\n}\n",
            (unsigned)g_loop_count,
            (unsigned)watch_app_runtime_get_stage_history_count(&state),
            page_name(g_ui_runtime.current));
    fclose(fp);
    return 0;
}

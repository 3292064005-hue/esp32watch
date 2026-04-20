#include <cassert>
#include <cstdio>
#include <cstring>

#include "web/web_state_bridge_internal.h"
#include "system_init.h"
#include "watch_app.h"
#include "services/time_service.h"

extern "C" bool web_wifi_connected(void) { return false; }
extern "C" bool web_wifi_access_point_active(void) { return false; }
extern "C" const char *web_wifi_access_point_ssid(void) { return "ESP32Watch"; }
extern "C" const char *web_wifi_mode_name(void) { return "STA"; }
extern "C" const char *web_wifi_status_name(void) { return "IDLE"; }
extern "C" const char *web_wifi_ip_string(void) { return "0.0.0.0"; }
extern "C" int32_t web_wifi_rssi(void) { return -42; }
const char *watch_app_init_stage_name(WatchAppInitStage stage)
{
    switch (stage) {
        case WATCH_APP_INIT_STAGE_NONE: return "NONE";
        case WATCH_APP_INIT_STAGE_MODEL: return "MODEL";
        default: return "OTHER";
    }
}
extern "C" const char *time_service_source_name(TimeSourceType source)
{
    switch (source) {
        case TIME_SOURCE_NETWORK_SYNC: return "NETWORK";
        case TIME_SOURCE_RECOVERY: return "RECOVERY";
        default: return "OTHER";
    }
}
extern "C" TimeInitStatus time_service_init_status(void)
{
    return TIME_INIT_STATUS_DEGRADED;
}
extern "C" const char *time_service_init_status_name(TimeInitStatus status)
{
    switch (status) {
        case TIME_INIT_STATUS_READY: return "READY";
        case TIME_INIT_STATUS_DEGRADED: return "DEGRADED";
        case TIME_INIT_STATUS_UNAVAILABLE: return "UNAVAILABLE";
        default: return "UNKNOWN";
    }
}
extern "C" const char *time_service_init_reason(void)
{
    return "RECOVERY_FLOOR";
}
extern "C" TimeAuthorityLevel time_service_authority_level(void)
{
    return TIME_AUTHORITY_RECOVERY;
}
extern "C" const char *time_service_authority_name(TimeAuthorityLevel authority)
{
    switch (authority) {
        case TIME_AUTHORITY_RECOVERY: return "RECOVERY";
        case TIME_AUTHORITY_NETWORK: return "NETWORK";
        case TIME_AUTHORITY_HARDWARE: return "HARDWARE";
        case TIME_AUTHORITY_HOST: return "HOST";
        default: return "NONE";
    }
}
extern "C" const char *time_service_confidence_name(TimeConfidenceLevel confidence)
{
    switch (confidence) {
        case TIME_CONFIDENCE_HIGH: return "HIGH";
        case TIME_CONFIDENCE_LOW: return "LOW";
        default: return "NONE";
    }
}
extern "C" const char *system_init_stage_name(SystemInitStage stage)
{
    switch (stage) {
        case SYSTEM_INIT_STAGE_NONE: return "NONE";
        case SYSTEM_INIT_STAGE_GPIO: return "GPIO";
        case SYSTEM_INIT_STAGE_WEB_SERVER: return "WEB_SERVER";
        case SYSTEM_INIT_STAGE_STORAGE_SERVICE: return "STORAGE_SERVICE";
        default: return "OTHER";
    }
}
void ui_status_compose_header_tags(char *out, size_t out_size)
{
    if (out != nullptr && out_size > 0U) {
        std::snprintf(out, out_size, "HDR");
    }
}
void ui_status_compose_network_value(char *line, size_t line_size, char *subline, size_t subline_size)
{
    if (line != nullptr && line_size > 0U) {
        std::snprintf(line, line_size, "NET");
    }
    if (subline != nullptr && subline_size > 0U) {
        std::snprintf(subline, subline_size, "SUB");
    }
}
void ui_status_compose_alarm_value(char *out, size_t out_size) { if (out && out_size) std::snprintf(out, out_size, "AL"); }
void ui_status_compose_music_value(char *out, size_t out_size) { if (out && out_size) std::snprintf(out, out_size, "MU"); }
void ui_status_compose_sensor_value(char *out, size_t out_size) { if (out && out_size) std::snprintf(out, out_size, "SE"); }
void ui_status_compose_storage_value(char *out, size_t out_size) { if (out && out_size) std::snprintf(out, out_size, "ST"); }
void ui_status_compose_diag_value(char *out, size_t out_size) { if (out && out_size) std::snprintf(out, out_size, "DI"); }
extern "C" bool web_runtime_snapshot_collect(WebRuntimeSnapshot *out) { (void)out; return false; }

int main()
{
    WebRuntimeSnapshot snapshot = {};
    WebStateCoreSnapshot core = {};

    snapshot.has_startup_report = true;
    snapshot.has_startup_status = true;
    snapshot.startup_report.startup_ok = false;
    snapshot.startup_report.degraded = true;
    snapshot.startup_report.fatal_stop_requested = true;
    snapshot.startup_report.failure_stage = SYSTEM_INIT_STAGE_WEB_SERVER;
    snapshot.startup_report.recovery_stage = SYSTEM_INIT_STAGE_STORAGE_SERVICE;
    snapshot.startup_status.init_failed = true;
    snapshot.startup_status.last_stage = SYSTEM_INIT_STAGE_GPIO;
    snapshot.startup_status.safe_mode_boot_recovery_pending = false;
    assert(web_state_core_collect_from_runtime_snapshot(&snapshot, &core));
    assert(!core.system.startup_ok);
    assert(core.system.startup_degraded);
    assert(core.system.fatal_stop_requested);
    assert(std::strcmp(core.system.startup_failure_stage, "WEB_SERVER") == 0);
    assert(std::strcmp(core.system.startup_recovery_stage, "STORAGE_SERVICE") == 0);

    std::memset(&snapshot, 0, sizeof(snapshot));
    std::memset(&core, 0, sizeof(core));
    snapshot.has_startup_status = true;
    snapshot.startup_status.init_failed = true;
    snapshot.startup_status.last_stage = SYSTEM_INIT_STAGE_GPIO;
    snapshot.startup_status.safe_mode_boot_recovery_pending = true;
    snapshot.startup_status.safe_mode_boot_recovery_stage = SYSTEM_INIT_STAGE_STORAGE_SERVICE;
    assert(web_state_core_collect_from_runtime_snapshot(&snapshot, &core));
    assert(!core.system.startup_ok);
    assert(core.system.startup_degraded);
    assert(core.system.fatal_stop_requested);
    assert(std::strcmp(core.system.startup_failure_stage, "UNKNOWN") == 0);
    assert(std::strcmp(core.system.startup_recovery_stage, "STORAGE_SERVICE") == 0);

    std::puts("[OK] web_state_core authoritative startup precedence check passed");
    return 0;
}

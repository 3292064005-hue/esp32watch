#pragma once
#include <stdbool.h>
#include "watch_app.h"
#include "model.h"
#include "services/device_config.h"
#include "services/network_sync_service.h"
#include "services/time_service.h"
#include "ui_internal.h"
#include "system_init.h"
#include "sensor.h"
#include "web/web_state_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool has_device_config;
    bool has_network_sync;
    bool has_time_source;
    bool has_ui_runtime;
    bool has_app_init_report;
    bool has_startup_report;
    bool has_startup_status;
    bool has_domain_state;
    bool has_sensor_snapshot;
    bool has_detail_state;
    bool web_control_plane_ready;
    bool web_console_ready;
    bool filesystem_ready;
    bool filesystem_assets_ready;
    bool asset_contract_ready;
    DeviceConfigSnapshot device_config;
    NetworkSyncSnapshot network_sync;
    TimeSourceSnapshot time_source;
    UiRuntimeSnapshot ui_runtime;
    WatchAppInitReport app_init_report;
    SystemStartupReport startup_report;
    SystemStartupStatus startup_status;
    ModelDomainState domain_state;
    SensorSnapshot sensor_snapshot;
    WebStateDetailSnapshot detail_state;
    char filesystem_status[24];
    uint32_t uptime_ms;
} WebRuntimeSnapshot;

/**
 * @brief Capture a unified runtime snapshot used by all web state serializers.
 *
 * Authoritative source map:
 * - Device provisioning/auth: storage facade backed by device_config
 * - Network sync and weather: network_sync_service
 * - Time provenance: time_service
 * - Current page/sleep animation state: ui runtime snapshot
 * - App/startup init state: watch_app + system_init
 * - Domain model values shown to users: model read snapshot
 * - Detail/perf/raw web fields: runtime snapshot detail_state subtree
 *
 * @param[out] out Destination aggregate snapshot.
 * @return true when @p out was populated.
 * @throws None.
 * @boundary_behavior Returns false for NULL @p out and otherwise leaves missing sub-snapshots marked through has_* flags instead of failing the whole capture.
 */
bool web_runtime_snapshot_collect(WebRuntimeSnapshot *out);

#ifdef __cplusplus
}
#endif

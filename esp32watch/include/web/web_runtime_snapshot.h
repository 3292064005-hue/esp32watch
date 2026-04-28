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
    bool from_published_snapshot;
    uint32_t published_at_ms;
    uint32_t snapshot_age_ms;
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

/**
 * @brief Publish the latest main-loop-owned runtime snapshot for Async web readers.
 *
 * @param[in] now_ms Current monotonic time used as the publication timestamp.
 * @return true when a new snapshot was captured and published.
 * @throws None.
 * @boundary_behavior Keeps the previous published snapshot when direct capture fails.
 */
bool web_runtime_snapshot_publish(uint32_t now_ms);

/**
 * @brief Copy the latest main-loop-published snapshot.
 *
 * @param[out] out Destination snapshot buffer.
 * @param[in] now_ms Current monotonic time used to compute snapshot age.
 * @return true when a published snapshot exists; false when the main loop has
 *         not published the first complete snapshot yet. Async web request
 *         paths must not perform direct runtime collection as a fallback.
 * @throws None.
 * @boundary_behavior Copies the active published buffer outside the critical
 *                    section after incrementing a per-buffer reader reference.
 */
bool web_runtime_snapshot_get_published(WebRuntimeSnapshot *out, uint32_t now_ms);


#ifdef __cplusplus
}
#endif

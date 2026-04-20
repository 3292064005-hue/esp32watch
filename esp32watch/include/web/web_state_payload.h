#ifndef WEB_STATE_PAYLOAD_H
#define WEB_STATE_PAYLOAD_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <stddef.h>

extern "C" {
#include "services/device_config.h"
}

#include "web/web_runtime_snapshot.h"
#include "web/web_state_bridge.h"
#include "web/web_state_bridge_internal.h"

typedef struct {
    WebRuntimeSnapshot runtime;
    WebStateCoreSnapshot core;
    WebStateDetailSnapshot detail;
    const DeviceConfigSnapshot *cfg;
} WebApiStateBundle;

typedef enum {
    WEB_STATE_PAYLOAD_SUMMARY = 0,
    WEB_STATE_PAYLOAD_DETAIL,
    WEB_STATE_PAYLOAD_PERF,
    WEB_STATE_PAYLOAD_RAW,
    WEB_STATE_PAYLOAD_AGGREGATE,
} WebStatePayloadView;

bool web_collect_api_state_bundle_impl(WebApiStateBundle *out, bool require_device_config);
void web_append_state_versions_impl(String &response);
void web_send_state_payload_response(AsyncWebServerRequest *request,
                                     WebStatePayloadView view,
                                     bool require_device_config,
                                     size_t reserve_hint);

#endif

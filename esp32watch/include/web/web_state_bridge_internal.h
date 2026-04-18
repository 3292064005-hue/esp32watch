#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "web/web_state_bridge.h"
#include "web/web_runtime_snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t web_state_bridge_uptime_ms(void);
bool web_state_core_collect_from_runtime_snapshot(const WebRuntimeSnapshot *runtime, WebStateCoreSnapshot *out);
bool web_state_detail_collect_from_runtime_snapshot(const WebRuntimeSnapshot *runtime, WebStateDetailSnapshot *out);
bool web_state_perf_collect_from_runtime_snapshot(const WebRuntimeSnapshot *runtime, WebStatePerfSnapshot *out);
bool web_state_core_collect_impl(WebStateCoreSnapshot *out);
bool web_state_detail_collect_impl(WebStateDetailSnapshot *out);
bool web_state_perf_collect_impl(WebStatePerfSnapshot *out);

#ifdef __cplusplus
}
#endif

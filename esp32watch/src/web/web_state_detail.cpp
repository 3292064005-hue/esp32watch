#include "web/web_state_bridge_internal.h"
#include "web/web_runtime_snapshot.h"
#include <string.h>

extern "C" bool web_state_detail_collect_from_runtime_snapshot(const WebRuntimeSnapshot *runtime, WebStateDetailSnapshot *out)
{
    if (runtime == NULL || out == NULL || !runtime->has_detail_state) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    *out = runtime->detail_state;
    return true;
}

extern "C" bool web_state_detail_collect_impl(WebStateDetailSnapshot *out)
{
    WebRuntimeSnapshot runtime = {};

    if (out == NULL) {
        return false;
    }

    if (!web_runtime_snapshot_collect(&runtime)) {
        return false;
    }

    return web_state_detail_collect_from_runtime_snapshot(&runtime, out);
}

extern "C" bool web_state_perf_collect_from_runtime_snapshot(const WebRuntimeSnapshot *runtime, WebStatePerfSnapshot *out)
{
    if (runtime == NULL || out == NULL || !runtime->has_detail_state) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    *out = runtime->detail_state.perf;
    return true;
}

extern "C" bool web_state_perf_collect_impl(WebStatePerfSnapshot *out)
{
    WebRuntimeSnapshot runtime = {};

    if (out == NULL) {
        return false;
    }

    if (!web_runtime_snapshot_collect(&runtime)) {
        return false;
    }

    return web_state_perf_collect_from_runtime_snapshot(&runtime, out);
}

#include "web/web_state_bridge.h"
#include "web/web_state_bridge_internal.h"
#include "platform_api.h"

static uint32_t g_startup_ms = 0;

extern "C" void web_state_bridge_mark_startup(uint32_t mark_ms)
{
    if (g_startup_ms == 0U) {
        g_startup_ms = mark_ms;
    }
}

extern "C" uint32_t web_state_bridge_uptime_ms(void)
{
    uint32_t now_ms = platform_time_now_ms();
    if (g_startup_ms == 0U) {
        g_startup_ms = now_ms;
    }
    return now_ms - g_startup_ms;
}

extern "C" bool web_state_core_collect(WebStateCoreSnapshot *out)
{
    return web_state_core_collect_impl(out);
}

extern "C" bool web_state_detail_collect(WebStateDetailSnapshot *out)
{
    return web_state_detail_collect_impl(out);
}

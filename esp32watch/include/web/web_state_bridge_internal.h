#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "web/web_state_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t web_state_bridge_uptime_ms(void);
bool web_state_core_collect_impl(WebStateCoreSnapshot *out);
bool web_state_detail_collect_impl(WebStateDetailSnapshot *out);

#ifdef __cplusplus
}
#endif

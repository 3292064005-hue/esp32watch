#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool web_overlay_request_text(const char *text, uint32_t now_ms, uint32_t duration_ms);
void web_overlay_clear(void);
bool web_overlay_is_active(uint32_t now_ms);
bool web_overlay_render_if_active(uint32_t now_ms);
const char *web_overlay_last_text(void);
uint32_t web_overlay_expire_at_ms(void);

#ifdef __cplusplus
}
#endif

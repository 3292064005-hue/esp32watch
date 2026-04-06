#include "web/web_overlay.h"
#include <string.h>

extern "C" {
#include "display.h"
}

typedef struct {
    bool active;
    uint32_t expire_at_ms;
    char text[64];
} WebOverlayState;

static WebOverlayState g_overlay = {
    false,
    0,
    ""
};

bool web_overlay_request_text(const char *text, uint32_t now_ms, uint32_t duration_ms)
{
    if (!text || duration_ms == 0) {
        return false;
    }

    size_t text_len = strlen(text);
    if (text_len == 0 || text_len > 63) {
        return false;
    }

    for (size_t i = 0; i < text_len; ++i) {
        char c = text[i];
        if ((unsigned char)c < 32 && c != '\n' && c != '\r' && c != '\t') {
            return false;
        }
    }

    strncpy(g_overlay.text, text, sizeof(g_overlay.text) - 1);
    g_overlay.text[sizeof(g_overlay.text) - 1] = '\0';

    g_overlay.expire_at_ms = now_ms + duration_ms;
    g_overlay.active = true;

    return true;
}

void web_overlay_clear(void)
{
    g_overlay.active = false;
    g_overlay.expire_at_ms = 0;
    g_overlay.text[0] = '\0';
}

bool web_overlay_is_active(uint32_t now_ms)
{
    if (!g_overlay.active || now_ms >= g_overlay.expire_at_ms) {
        g_overlay.active = false;
        return false;
    }
    return true;
}

bool web_overlay_render_if_active(uint32_t now_ms)
{
    if (!web_overlay_is_active(now_ms)) {
        return false;
    }

    display_clear();
    display_draw_text_5x7(0, 0, g_overlay.text, true);
    display_present();

    return true;
}

const char *web_overlay_last_text(void)
{
    return g_overlay.text;
}

uint32_t web_overlay_expire_at_ms(void)
{
    return g_overlay.expire_at_ms;
}

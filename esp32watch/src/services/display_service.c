#include "services/display_service.h"
#include "display.h"
#include "display_internal.h"
#include "services/diag_service.h"
#include "power.h"
#include "platform_api.h"
#include <string.h>

static bool g_force_full_refresh;
static bool g_display_initialized;

static void display_boot_self_test(void)
{
    display_set_contrast(255U);
    memset(g_oled_buffer, 0xFF, sizeof(g_oled_buffer));
    (void)display_present();
    platform_delay_ms(500U);

    display_clear();
    (void)display_present();
    platform_delay_ms(150U);
}

void display_service_init(void)
{
    display_init();
    display_sleep(false);
    display_boot_self_test();
    g_force_full_refresh = true;
    g_display_initialized = true;
}

void display_service_apply_settings(const SettingsState *settings)
{
    display_set_contrast(power_brightness_to_contrast(settings->brightness));
}

void display_service_set_sleeping(bool sleeping)
{
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)sleeping;
    display_sleep(false);
#else
    display_sleep(sleeping);
    if (!sleeping) {
        g_force_full_refresh = true;
    }
#endif
}

void display_service_begin_frame(void)
{
    if (g_force_full_refresh) {
        display_invalidate_cache();
        display_core_note_full_refresh();
    }
    display_clear();
}

void display_service_end_frame(void)
{
    if (!display_present()) {
        g_force_full_refresh = true;
        diag_service_note_display_tx_fail(1U);
        return;
    }
    g_force_full_refresh = false;
}

void display_service_force_full_refresh(void)
{
    g_force_full_refresh = true;
}


bool display_service_is_initialized(void)
{
    return g_display_initialized;
}


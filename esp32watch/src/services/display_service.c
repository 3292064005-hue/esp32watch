#include "services/display_service.h"
#include "display.h"
#include "display_internal.h"
#include "services/diag_service.h"
#include "services/runtime_event_service.h"
#include "services/runtime_reload_coordinator.h"
#include "power.h"
#include "platform_api.h"
#include <string.h>

static bool g_force_full_refresh;
static bool g_display_initialized;
static uint32_t g_last_applied_generation;

static void display_service_note_applied_generation(void)
{
    g_last_applied_generation = device_config_generation();
}

static bool display_service_handle_runtime_event(RuntimeServiceEvent event, void *ctx)
{
    ModelDomainState domain_state = {0};
    (void)ctx;
    if (event != RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED) {
        return true;
    }
    if (!g_display_initialized || model_get_domain_state(&domain_state) == NULL) {
        return false;
    }
    display_service_apply_settings(&domain_state.settings);
    display_service_note_applied_generation();
    return true;
}

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
    RuntimeEventSubscription subscription = {
        .handler = display_service_handle_runtime_event,
        .ctx = NULL,
        .name = "display_service",
        .priority = 10,
        .critical = true,
        .event_mask = runtime_event_service_event_mask(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED),
        .domain_mask = RUNTIME_RELOAD_DOMAIN_DISPLAY,
    };

    display_init();
    display_sleep(false);
    display_boot_self_test();
    g_force_full_refresh = true;
    g_display_initialized = runtime_event_service_register_ex(&subscription);
    if (g_display_initialized) {
        display_service_note_applied_generation();
    }
}

void display_service_apply_settings(const SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
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


uint32_t display_service_last_applied_generation(void)
{
    return g_last_applied_generation;
}

bool display_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_display_initialized && generation != 0U && g_last_applied_generation == generation;
}

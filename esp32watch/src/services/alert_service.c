#include "services/alert_service.h"
#include "model.h"
#include "vibe.h"
#include <stddef.h>

static bool g_alert_service_initialized;

void alert_service_init(void)
{
    vibe_init();
    g_alert_service_initialized = true;
}

void alert_service_tick(uint32_t now_ms)
{
    ModelDomainState domain_state;
    ModelUiState ui_state;

    if (model_get_domain_state(&domain_state) == NULL || model_get_ui_state(&ui_state) == NULL) {
        return;
    }

    vibe_tick(now_ms, ui_state.popup, domain_state.settings.vibrate && !domain_state.settings.dnd);
}


bool alert_service_is_initialized(void)
{
    return g_alert_service_initialized;
}

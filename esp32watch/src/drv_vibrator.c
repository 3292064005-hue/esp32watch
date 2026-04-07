#include "drv_vibrator.h"
#include "board_manifest.h"
#include "bsp_gpio.h"
#include "platform_api.h"

static bool g_active;

void drv_vibrator_init(void)
{
    const BoardManifest *manifest = board_manifest_get();

    g_active = false;
    if (!manifest->vibration_enabled || manifest->vibe_port == NULL || manifest->vibe_pin == 0U) {
        return;
    }

    bsp_gpio_init_output_od(manifest->vibe_port, manifest->vibe_pin, PLATFORM_GPIO_SPEED_HIGH);
    bsp_gpio_write(manifest->vibe_port, manifest->vibe_pin, false);
}

void drv_vibrator_set(bool enabled)
{
    const BoardManifest *manifest = board_manifest_get();
    g_active = enabled;
    if (!manifest->vibration_enabled || manifest->vibe_port == NULL || manifest->vibe_pin == 0U) {
        return;
    }
    bsp_gpio_write(manifest->vibe_port, manifest->vibe_pin, enabled);
}

bool drv_vibrator_is_active(void)
{
    return g_active;
}

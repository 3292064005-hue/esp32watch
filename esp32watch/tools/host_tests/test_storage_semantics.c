#include "services/storage_semantics.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool g_flash_ready = false;

bool storage_service_is_flash_backend_ready(void)
{
    return g_flash_ready;
}

bool storage_service_app_state_durable_ready(void)
{
    return g_flash_ready;
}

int main(void)
{
    StorageObjectSemantic semantic = {0};

    assert(strcmp(storage_durability_level_name(STORAGE_DURABILITY_VOLATILE), "VOLATILE") == 0);
    assert(strcmp(storage_durability_level_name(STORAGE_DURABILITY_RESET_DOMAIN), "RESET_DOMAIN") == 0);
    assert(strcmp(storage_durability_level_name(STORAGE_DURABILITY_DURABLE), "DURABLE") == 0);

    g_flash_ready = false;
    assert(storage_semantics_describe(STORAGE_OBJECT_APP_SETTINGS, &semantic));
    assert(strcmp(semantic.name, "APP_SETTINGS") == 0);
    assert(semantic.managed_by_storage_service);
    assert(semantic.durability == STORAGE_DURABILITY_RESET_DOMAIN);
    assert(!semantic.survives_power_loss);

    g_flash_ready = true;
    assert(storage_semantics_describe(STORAGE_OBJECT_SENSOR_CALIBRATION, &semantic));
    assert(semantic.durability == STORAGE_DURABILITY_DURABLE);
    assert(semantic.survives_power_loss);

    assert(storage_semantics_describe(STORAGE_OBJECT_DEVICE_CONFIG, &semantic));
    assert(strcmp(semantic.owner, "device_config") == 0);
    assert(semantic.durability == STORAGE_DURABILITY_DURABLE);
    assert(semantic.survives_power_loss);

    assert(!storage_semantics_describe(STORAGE_OBJECT_COUNT, &semantic));
    assert(!storage_semantics_describe(STORAGE_OBJECT_APP_SETTINGS, NULL));

    puts("[OK] storage_semantics behavior check passed");
    return 0;
}

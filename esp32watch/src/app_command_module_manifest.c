#include "app_command_module_manifest.h"

void app_command_module_manifest_register_providers(void)
{
    (void)app_command_registry_register_provider("core", app_command_register_core_module);
}

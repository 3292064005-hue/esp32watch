#include "web/web_route_module_manifest.h"
#include "web/web_route_module_registry.h"

void web_route_module_manifest_register_providers(void)
{
    (void)web_route_module_register_provider("core", web_install_route_module_core);
    (void)web_route_module_register_provider("actions", web_install_route_module_actions);
    (void)web_route_module_register_provider("config", web_install_route_module_config);
    (void)web_route_module_register_provider("reset", web_install_route_module_reset);
    (void)web_route_module_register_provider("state", web_install_route_module_state);
    (void)web_route_module_register_provider("display", web_install_route_module_display);
    (void)web_route_module_register_provider("input", web_install_route_module_input);
}

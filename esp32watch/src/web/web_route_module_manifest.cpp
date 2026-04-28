#include "web/web_route_module_manifest.h"
#include "web/web_route_module_registry.h"

struct WebRouteModuleProviderManifestEntry {
    const char *name;
    WebRouteModuleInstaller installer;
};

static const WebRouteModuleProviderManifestEntry kWebRouteModuleProviders[] = {
    {"core", web_install_route_module_core},
    {"actions", web_install_route_module_actions},
    {"config", web_install_route_module_config},
    {"reset", web_install_route_module_reset},
    {"state", web_install_route_module_state},
    {"display", web_install_route_module_display},
    {"input", web_install_route_module_input},
};

static_assert((sizeof(kWebRouteModuleProviders) / sizeof(kWebRouteModuleProviders[0])) == WEB_ROUTE_MODULE_MANIFEST_PROVIDER_COUNT,
              "WEB_ROUTE_MODULE_MANIFEST_PROVIDER_COUNT must match kWebRouteModuleProviders");

size_t web_route_module_manifest_provider_count(void)
{
    return sizeof(kWebRouteModuleProviders) / sizeof(kWebRouteModuleProviders[0]);
}

void web_route_module_manifest_register_providers(void)
{
    for (size_t i = 0U; i < web_route_module_manifest_provider_count(); ++i) {
        (void)web_route_module_register_provider(kWebRouteModuleProviders[i].name, kWebRouteModuleProviders[i].installer);
    }
}

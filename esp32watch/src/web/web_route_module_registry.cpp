#include "web/web_route_module_registry.h"
#include "web/web_route_module_manifest.h"
#include <cstring>

namespace {
constexpr size_t kWebRouteProviderCapacity = WEB_ROUTE_MODULE_MANIFEST_PROVIDER_COUNT;
constexpr size_t kWebRouteModuleCapacity = WEB_ROUTE_MODULE_MANIFEST_PROVIDER_COUNT;
static_assert(kWebRouteProviderCapacity >= WEB_ROUTE_MODULE_MANIFEST_PROVIDER_COUNT,
              "route provider registry capacity must cover the provider manifest");

WebRouteModuleDescriptor g_modules[kWebRouteModuleCapacity] = {};
size_t g_module_count = 0U;

struct WebRouteModuleProviderEntry {
    const char *name;
    WebRouteModuleInstaller installer;
};

WebRouteModuleProviderEntry g_providers[kWebRouteProviderCapacity] = {};
size_t g_provider_count = 0U;
bool g_initialized = false;
}

void web_route_module_registry_reset(void)
{
    std::memset(g_modules, 0, sizeof(g_modules));
    std::memset(g_providers, 0, sizeof(g_providers));
    g_module_count = 0U;
    g_provider_count = 0U;
    g_initialized = false;
}

bool web_route_module_register(const char *name,
                               void (*register_fn)(AsyncWebServer &server),
                               const WebRouteCatalogEntry *routes,
                               size_t route_count,
                               const WebApiSchemaCatalogEntry *api_schemas,
                               size_t api_schema_count,
                               const WebStateSchemaCatalogEntry *state_schemas,
                               size_t state_schema_count)
{
    if (name == nullptr || register_fn == nullptr ||
        (route_count != 0U && routes == nullptr) ||
        (api_schema_count != 0U && api_schemas == nullptr) ||
        (state_schema_count != 0U && state_schemas == nullptr)) {
        return false;
    }

    for (size_t i = 0U; i < g_module_count; ++i) {
        if (g_modules[i].register_fn == register_fn || std::strcmp(g_modules[i].name, name) == 0) {
            return true;
        }
    }

    if (g_module_count >= kWebRouteModuleCapacity) {
        return false;
    }

    WebRouteModuleDescriptor &module = g_modules[g_module_count++];
    module.name = name;
    module.register_fn = register_fn;
    module.routes = routes;
    module.route_count = route_count;
    module.api_schemas = api_schemas;
    module.api_schema_count = api_schema_count;
    module.state_schemas = state_schemas;
    module.state_schema_count = state_schema_count;
    return true;
}

bool web_route_module_register_provider(const char *name, WebRouteModuleInstaller installer)
{
    if (name == nullptr || name[0] == '\0' || installer == nullptr) {
        return false;
    }
    for (size_t i = 0U; i < g_provider_count; ++i) {
        if (g_providers[i].installer == installer || std::strcmp(g_providers[i].name, name) == 0) {
            return true;
        }
    }
    if (g_provider_count >= kWebRouteProviderCapacity) {
        return false;
    }
    g_providers[g_provider_count++] = {name, installer};
    return true;
}

void web_route_module_registry_ensure_initialized(void)
{
    if (g_initialized) {
        return;
    }
    g_initialized = true;
    web_route_module_manifest_register_providers();
    for (size_t i = 0U; i < g_provider_count; ++i) {
        g_providers[i].installer();
    }
}

size_t web_route_module_count(void)
{
    web_route_module_registry_ensure_initialized();
    return g_module_count;
}

const WebRouteModuleDescriptor *web_route_module_at(size_t index)
{
    web_route_module_registry_ensure_initialized();
    if (index >= g_module_count) {
        return nullptr;
    }
    return &g_modules[index];
}

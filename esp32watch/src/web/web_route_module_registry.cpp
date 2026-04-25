#include "web/web_route_module_registry.h"
#include "web/web_route_module_manifest.h"
#include <cstdlib>
#include <cstring>

namespace {
struct WebRouteModuleNode {
    WebRouteModuleDescriptor module;
    WebRouteModuleNode *next;
};

struct WebRouteModuleProviderNode {
    const char *name;
    WebRouteModuleInstaller installer;
    WebRouteModuleProviderNode *next;
};

WebRouteModuleNode *g_module_head = nullptr;
WebRouteModuleNode *g_module_tail = nullptr;
size_t g_module_count = 0U;
WebRouteModuleProviderNode *g_provider_head = nullptr;
WebRouteModuleProviderNode *g_provider_tail = nullptr;
bool g_initialized = false;

void free_modules()
{
    WebRouteModuleNode *node = g_module_head;
    while (node != nullptr) {
        WebRouteModuleNode *next = node->next;
        std::free(node);
        node = next;
    }
    g_module_head = nullptr;
    g_module_tail = nullptr;
    g_module_count = 0U;
}
}

void web_route_module_registry_reset(void)
{
    free_modules();
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
    WebRouteModuleNode *node;
    if (name == nullptr || register_fn == nullptr ||
        (route_count != 0U && routes == nullptr) ||
        (api_schema_count != 0U && api_schemas == nullptr) ||
        (state_schema_count != 0U && state_schemas == nullptr)) {
        return false;
    }
    node = static_cast<WebRouteModuleNode *>(std::calloc(1U, sizeof(*node)));
    if (node == nullptr) {
        return false;
    }
    node->module.name = name;
    node->module.register_fn = register_fn;
    node->module.routes = routes;
    node->module.route_count = route_count;
    node->module.api_schemas = api_schemas;
    node->module.api_schema_count = api_schema_count;
    node->module.state_schemas = state_schemas;
    node->module.state_schema_count = state_schema_count;
    if (g_module_tail != nullptr) {
        g_module_tail->next = node;
    } else {
        g_module_head = node;
    }
    g_module_tail = node;
    ++g_module_count;
    return true;
}

bool web_route_module_register_provider(const char *name, WebRouteModuleInstaller installer)
{
    WebRouteModuleProviderNode *cursor;
    WebRouteModuleProviderNode *node;
    if (name == nullptr || name[0] == '\0' || installer == nullptr) {
        return false;
    }
    for (cursor = g_provider_head; cursor != nullptr; cursor = cursor->next) {
        if (cursor->installer == installer || std::strcmp(cursor->name, name) == 0) {
            return true;
        }
    }
    node = static_cast<WebRouteModuleProviderNode *>(std::calloc(1U, sizeof(*node)));
    if (node == nullptr) {
        return false;
    }
    node->name = name;
    node->installer = installer;
    if (g_provider_tail != nullptr) {
        g_provider_tail->next = node;
    } else {
        g_provider_head = node;
    }
    g_provider_tail = node;
    return true;
}

void web_route_module_registry_ensure_initialized(void)
{
    WebRouteModuleProviderNode *provider;
    if (g_initialized) {
        return;
    }
    g_initialized = true;
    web_route_module_manifest_register_providers();
    for (provider = g_provider_head; provider != nullptr; provider = provider->next) {
        provider->installer();
    }
}

size_t web_route_module_count(void)
{
    web_route_module_registry_ensure_initialized();
    return g_module_count;
}

const WebRouteModuleDescriptor *web_route_module_at(size_t index)
{
    size_t current = 0U;
    WebRouteModuleNode *node;
    web_route_module_registry_ensure_initialized();
    for (node = g_module_head; node != nullptr; node = node->next) {
        if (current == index) {
            return &node->module;
        }
        ++current;
    }
    return nullptr;
}

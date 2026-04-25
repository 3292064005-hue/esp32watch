#include "web/web_route_catalog_registry.h"
#include "web/web_route_module_registry.h"
#include <string.h>

namespace {
const char *normalise_method(const char *method)
{
    return (method != nullptr && method[0] != '\0') ? method : "GET";
}

bool method_equals(const char *lhs, const char *rhs)
{
    return lhs != nullptr && rhs != nullptr && strcmp(lhs, rhs) == 0;
}
}

size_t web_route_catalog_count(void)
{
    size_t total = 0U;
    for (size_t module_index = 0U; module_index < web_route_module_count(); ++module_index) {
        const WebRouteModuleDescriptor *module = web_route_module_at(module_index);
        if (module != nullptr) {
            total += module->route_count;
        }
    }
    return total;
}

const WebRouteCatalogEntry *web_route_catalog_at(size_t index)
{
    size_t base = 0U;
    for (size_t module_index = 0U; module_index < web_route_module_count(); ++module_index) {
        const WebRouteModuleDescriptor *module = web_route_module_at(module_index);
        if (module == nullptr || module->routes == nullptr) {
            continue;
        }
        if (index < base + module->route_count) {
            return &module->routes[index - base];
        }
        base += module->route_count;
    }
    return nullptr;
}

const WebRouteCatalogEntry *web_route_catalog_find(const char *route_key)
{
    if (route_key == nullptr) {
        return nullptr;
    }
    for (size_t i = 0U; i < web_route_catalog_count(); ++i) {
        const WebRouteCatalogEntry *route = web_route_catalog_at(i);
        if (route != nullptr && route->route_key != nullptr && strcmp(route->route_key, route_key) == 0) {
            return route;
        }
    }
    return nullptr;
}

const WebRouteOperationCatalogEntry *web_route_catalog_operation_for_method(const WebRouteCatalogEntry *route,
                                                                            const char *method)
{
    const char *wanted = normalise_method(method);
    if (route == nullptr || route->operations == nullptr) {
        return nullptr;
    }
    for (size_t i = 0U; i < route->operation_count; ++i) {
        const WebRouteOperationCatalogEntry *operation = &route->operations[i];
        if (method_equals(operation->method, wanted)) {
            return operation;
        }
    }
    return nullptr;
}

const WebRouteOperationCatalogEntry *web_route_catalog_default_operation(const WebRouteCatalogEntry *route)
{
    if (route == nullptr) {
        return nullptr;
    }
    const WebRouteOperationCatalogEntry *operation = web_route_catalog_operation_for_method(route, route->default_method);
    if (operation != nullptr) {
        return operation;
    }
    return route->operation_count > 0U ? &route->operations[0] : nullptr;
}

size_t web_api_schema_catalog_count(void)
{
    size_t total = 0U;
    for (size_t module_index = 0U; module_index < web_route_module_count(); ++module_index) {
        const WebRouteModuleDescriptor *module = web_route_module_at(module_index);
        if (module != nullptr) {
            total += module->api_schema_count;
        }
    }
    return total;
}

const WebApiSchemaCatalogEntry *web_api_schema_catalog_at(size_t index)
{
    size_t base = 0U;
    for (size_t module_index = 0U; module_index < web_route_module_count(); ++module_index) {
        const WebRouteModuleDescriptor *module = web_route_module_at(module_index);
        if (module == nullptr || module->api_schemas == nullptr) {
            continue;
        }
        if (index < base + module->api_schema_count) {
            return &module->api_schemas[index - base];
        }
        base += module->api_schema_count;
    }
    return nullptr;
}

const WebApiSchemaCatalogEntry *web_api_schema_catalog_find(const char *name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (size_t i = 0U; i < web_api_schema_catalog_count(); ++i) {
        const WebApiSchemaCatalogEntry *schema = web_api_schema_catalog_at(i);
        if (schema != nullptr && schema->name != nullptr && strcmp(schema->name, name) == 0) {
            return schema;
        }
    }
    return nullptr;
}

size_t web_state_schema_catalog_count(void)
{
    size_t total = 0U;
    for (size_t module_index = 0U; module_index < web_route_module_count(); ++module_index) {
        const WebRouteModuleDescriptor *module = web_route_module_at(module_index);
        if (module != nullptr) {
            total += module->state_schema_count;
        }
    }
    return total;
}

const WebStateSchemaCatalogEntry *web_state_schema_catalog_at(size_t index)
{
    size_t base = 0U;
    for (size_t module_index = 0U; module_index < web_route_module_count(); ++module_index) {
        const WebRouteModuleDescriptor *module = web_route_module_at(module_index);
        if (module == nullptr || module->state_schemas == nullptr) {
            continue;
        }
        if (index < base + module->state_schema_count) {
            return &module->state_schemas[index - base];
        }
        base += module->state_schema_count;
    }
    return nullptr;
}

const WebStateSchemaCatalogEntry *web_state_schema_catalog_find(const char *name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (size_t i = 0U; i < web_state_schema_catalog_count(); ++i) {
        const WebStateSchemaCatalogEntry *schema = web_state_schema_catalog_at(i);
        if (schema != nullptr && schema->name != nullptr && strcmp(schema->name, name) == 0) {
            return schema;
        }
    }
    return nullptr;
}

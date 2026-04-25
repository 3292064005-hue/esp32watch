#include "web/web_contract.h"
#include "web/web_json.h"
#include "web/web_route_catalog_registry.h"
#include <Arduino.h>
#include <string.h>

namespace {
static void append_route(String &response, const char *key, const char *value, bool trailing)
{
    web_json_kv_str(response, key, value, trailing);
}

static void append_string_array(String &response, const char *const *values, size_t count)
{
    response += '[';
    for (size_t i = 0; i < count; ++i) {
        if (i != 0U) {
            response += ',';
        }
        response += '"';
        web_json_escape_append(response, values[i]);
        response += '"';
    }
    response += ']';
}

static void append_state_schema_entries(String &response,
                                        const WebStateSectionCatalogEntry *entries,
                                        size_t count)
{
    response += '[';
    for (size_t i = 0U; i < count; ++i) {
        if (i != 0U) {
            response += ',';
        }
        response += '{';
        web_json_kv_str(response, "section", entries[i].section, false);
        response += "\"flags\":";
        append_string_array(response, entries[i].flags, entries[i].flag_count);
        response += '}';
    }
    response += ']';
}

static void append_api_schema(String &response, const WebApiSchemaCatalogEntry &schema, bool trailing)
{
    response += '"';
    response += schema.name;
    response += "\":{";
    web_json_kv_str(response, "type", "object", false);
    response += "\"required\":";
    append_string_array(response, schema.required, schema.required_count);
    response += ',';
    response += "\"sections\":";
    append_string_array(response, schema.sections, schema.section_count);
    response += '}';
    if (!trailing) {
        response += ',';
    }
}

static void append_operation_schema(String &response, const WebRouteOperationCatalogEntry &operation)
{
    response += '{';
    web_json_kv_str(response, "kind", operation.schema_kind, false);
    web_json_kv_str(response, "name", operation.schema_name, false);
    response += "\"requestFields\":";
    append_string_array(response, operation.request_fields, operation.request_field_count);
    response += ',';
    response += "\"responseFields\":";
    append_string_array(response, operation.response_fields, operation.response_field_count);
    response += '}';
}

static void append_route_operations(String &response, const WebRouteCatalogEntry &route)
{
    response += "\"operations\":{";
    for (size_t i = 0U; i < route.operation_count; ++i) {
        if (i != 0U) {
            response += ',';
        }
        response += '"';
        web_json_escape_append(response, route.operations[i].method);
        response += "\":";
        append_operation_schema(response, route.operations[i]);
    }
    response += '}';
}

static void append_state_schemas(String &response)
{
    response += "\"stateSchemas\":{";
    for (size_t i = 0U; i < web_state_schema_catalog_count(); ++i) {
        const WebStateSchemaCatalogEntry *schema = web_state_schema_catalog_at(i);
        if (schema == nullptr) {
            continue;
        }
        if (i != 0U) {
            response += ',';
        }
        response += '"';
        web_json_escape_append(response, schema->name);
        response += "\":";
        append_state_schema_entries(response, schema->sections, schema->section_count);
    }
    response += '}';
}

static void append_release_validation_schema(String &response)
{
    response += "\"releaseValidation\":{";
    web_json_kv_str(response, "candidateBundleKind", "CANDIDATE_RELEASE_BUNDLE", false);
    web_json_kv_str(response, "verifiedBundleKind", "VERIFIED_RELEASE_BUNDLE", false);
    web_json_kv_str(response, "hostReportType", "HOST_VALIDATION_REPORT", false);
    web_json_kv_str(response, "deviceReportType", "DEVICE_SMOKE_REPORT", false);
    web_json_kv_u32(response, "validationSchemaVersion", WEB_RELEASE_VALIDATION_SCHEMA_VERSION, true);
    response += '}';
}

static void append_api_schemas(String &response)
{
    response += "\"apiSchemas\":{";
    const size_t count = web_api_schema_catalog_count();
    for (size_t i = 0U; i < count; ++i) {
        const WebApiSchemaCatalogEntry *schema = web_api_schema_catalog_at(i);
        if (schema == nullptr) {
            continue;
        }
        append_api_schema(response, *schema, i + 1U == count);
    }
    response += '}';
}

static void append_route_schemas(String &response)
{
    bool first = true;
    response += "\"routeSchemas\":{";
    for (size_t route_index = 0U; route_index < web_route_catalog_count(); ++route_index) {
        const WebRouteCatalogEntry *route_ptr = web_route_catalog_at(route_index);
        if (route_ptr == nullptr) {
            continue;
        }
        const WebRouteCatalogEntry &route = *route_ptr;
        const WebRouteOperationCatalogEntry *default_operation = web_route_catalog_default_operation(&route);
        if (default_operation == nullptr) {
            continue;
        }
        if (!first) {
            response += ',';
        }
        first = false;
        response += '"';
        response += route.route_key;
        response += "\":{";
        web_json_kv_str(response, "kind", default_operation->schema_kind, false);
        web_json_kv_str(response, "name", default_operation->schema_name, false);
        web_json_kv_str(response, "defaultMethod", route.default_method, false);
        web_json_kv_str(response, "tier", route.surface_tier, false);
        web_json_kv_str(response, "producerOwner", route.producer_owner, false);
        web_json_kv_str(response, "consumerOwner", route.consumer_owner, false);
        web_json_kv_str(response, "deprecationPolicy", route.deprecation_policy, false);
        response += "\"requestFields\":";
        append_string_array(response, default_operation->request_fields, default_operation->request_field_count);
        response += ',';
        response += "\"responseFields\":";
        append_string_array(response, default_operation->response_fields, default_operation->response_field_count);
        response += ',';
        append_route_operations(response, route);
        response += '}';
    }
    response += '}';
}

static bool copy_api_schema(const WebApiSchemaCatalogEntry *schema, WebApiSchemaView *out)
{
    if (schema == nullptr || out == nullptr) {
        return false;
    }
    out->name = schema->name;
    out->required = schema->required;
    out->required_count = schema->required_count;
    out->sections = schema->sections;
    out->section_count = schema->section_count;
    return true;
}

static bool copy_route_schema(const WebRouteCatalogEntry *route,
                              const WebRouteOperationCatalogEntry *operation,
                              WebRouteSchemaView *out)
{
    if (route == nullptr || operation == nullptr || out == nullptr) {
        return false;
    }
    out->route_key = route->route_key;
    out->method = operation->method;
    out->schema_kind = operation->schema_kind;
    out->schema_name = operation->schema_name;
    out->surface_tier = route->surface_tier;
    out->producer_owner = route->producer_owner;
    out->consumer_owner = route->consumer_owner;
    out->deprecation_policy = route->deprecation_policy;
    out->request_fields = operation->request_fields;
    out->request_field_count = operation->request_field_count;
    out->response_fields = operation->response_fields;
    out->response_field_count = operation->response_field_count;
    return true;
}
} // namespace

bool web_contract_get_api_schema(const char *name, WebApiSchemaView *out)
{
    return copy_api_schema(web_api_schema_catalog_find(name), out);
}

bool web_contract_get_route_schema_for_method(const char *route_key, const char *method, WebRouteSchemaView *out)
{
    const WebRouteCatalogEntry *route = web_route_catalog_find(route_key);
    const WebRouteOperationCatalogEntry *operation = web_route_catalog_operation_for_method(route, method);
    return copy_route_schema(route, operation, out);
}

bool web_contract_get_route_schema(const char *route_key, WebRouteSchemaView *out)
{
    const WebRouteCatalogEntry *route = web_route_catalog_find(route_key);
    return copy_route_schema(route, web_route_catalog_default_operation(route), out);
}

bool web_contract_get_route_api_schema_for_method(const char *route_key, const char *method, WebApiSchemaView *out)
{
    WebRouteSchemaView route = {};
    if (!web_contract_get_route_schema_for_method(route_key, method, &route)) {
        return false;
    }
    if (strcmp(route.schema_kind, "api") != 0) {
        return false;
    }
    return web_contract_get_api_schema(route.schema_name, out);
}

bool web_contract_get_route_api_schema(const char *route_key, WebApiSchemaView *out)
{
    WebRouteSchemaView route = {};
    if (!web_contract_get_route_schema(route_key, &route)) {
        return false;
    }
    if (strcmp(route.schema_kind, "api") != 0) {
        return false;
    }
    return web_contract_get_api_schema(route.schema_name, out);
}

void web_contract_append_json(String &response)
{
    bool first = true;
    response += "\"contract\":{";
    web_json_kv_u32(response, "runtimeContractVersion", WEB_RUNTIME_CONTRACT_VERSION, false);
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    web_json_kv_u32(response, "stateVersion", WEB_STATE_VERSION, false);
    web_json_kv_u32(response, "commandCatalogVersion", WEB_COMMAND_CATALOG_VERSION, false);
    web_json_kv_u32(response, "assetContractVersion", WEB_ASSET_CONTRACT_VERSION, false);
    response += "\"routes\":{";
    for (size_t route_index = 0U; route_index < web_route_catalog_count(); ++route_index) {
        const WebRouteCatalogEntry *route_ptr = web_route_catalog_at(route_index);
        if (route_ptr == nullptr) {
            continue;
        }
        const WebRouteCatalogEntry &route = *route_ptr;
        if (!first) {
            response += ',';
        }
        first = false;
        append_route(response, route.route_key, route.route_path, true);
    }
    response += "},";
    append_route_schemas(response);
    response += ',';
    append_state_schemas(response);
    response += ',';
    append_release_validation_schema(response);
    response += ',';
    append_api_schemas(response);
    response += '}';
}

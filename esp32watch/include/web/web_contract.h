#ifndef WEB_CONTRACT_H
#define WEB_CONTRACT_H

#include <stdint.h>
#include <stddef.h>

class String;

static constexpr uint32_t WEB_API_VERSION = 20U;
static constexpr uint32_t WEB_STATE_VERSION = 13U;
static constexpr uint32_t WEB_COMMAND_CATALOG_VERSION = 4U;
static constexpr uint32_t WEB_RUNTIME_CONTRACT_VERSION = 15U;
static constexpr uint32_t WEB_ASSET_CONTRACT_VERSION = 15U;
static constexpr uint32_t WEB_RELEASE_VALIDATION_SCHEMA_VERSION = 8U;

static constexpr const char *WEB_ROUTE_CONTRACT = "/api/contract";
static constexpr const char *WEB_ROUTE_HEALTH = "/api/health";
static constexpr const char *WEB_ROUTE_META = "/api/meta";
static constexpr const char *WEB_ROUTE_ACTIONS_CATALOG = "/api/actions/catalog";
static constexpr const char *WEB_ROUTE_ACTIONS_LATEST = "/api/actions/latest";
static constexpr const char *WEB_ROUTE_ACTIONS_STATUS = "/api/actions/status";
static constexpr const char *WEB_ROUTE_STATE_DETAIL = "/api/state/detail";
static constexpr const char *WEB_ROUTE_STATE_PERF = "/api/state/perf";
static constexpr const char *WEB_ROUTE_STATE_RAW = "/api/state/raw";
static constexpr const char *WEB_ROUTE_STATE_AGGREGATE = "/api/state/aggregate";
static constexpr const char *WEB_ROUTE_DISPLAY_FRAME = "/api/display/frame";
static constexpr const char *WEB_ROUTE_DISPLAY_OVERLAY = "/api/display/overlay";
static constexpr const char *WEB_ROUTE_DISPLAY_OVERLAY_CLEAR = "/api/display/overlay/clear";
static constexpr const char *WEB_ROUTE_CONFIG_DEVICE = "/api/config/device";
static constexpr const char *WEB_ROUTE_INPUT_KEY = "/api/input/key";
static constexpr const char *WEB_ROUTE_COMMAND = "/api/command";
static constexpr const char *WEB_ROUTE_RESET_APP_STATE = "/api/reset/app-state";
static constexpr const char *WEB_ROUTE_RESET_DEVICE_CONFIG = "/api/reset/device-config";
static constexpr const char *WEB_ROUTE_RESET_FACTORY = "/api/reset/factory";
static constexpr const char *WEB_ROUTE_STORAGE_SEMANTICS = "/api/storage/semantics";


struct WebApiSchemaView {
    const char *name;
    const char *const *required;
    size_t required_count;
    const char *const *sections;
    size_t section_count;
};

struct WebRouteSchemaView {
    const char *route_key;
    const char *method;
    const char *schema_kind;
    const char *schema_name;
    const char *surface_tier;
    const char *producer_owner;
    const char *consumer_owner;
    const char *deprecation_policy;
    const char *const *request_fields;
    size_t request_field_count;
    const char *const *response_fields;
    size_t response_field_count;
};

bool web_contract_get_api_schema(const char *name, WebApiSchemaView *out);
bool web_contract_get_route_schema(const char *route_key, WebRouteSchemaView *out);
bool web_contract_get_route_schema_for_method(const char *route_key, const char *method, WebRouteSchemaView *out);
bool web_contract_get_route_api_schema(const char *route_key, WebApiSchemaView *out);
bool web_contract_get_route_api_schema_for_method(const char *route_key, const char *method, WebApiSchemaView *out);
void web_contract_append_json(String &response);

#endif

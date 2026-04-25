#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

namespace {
static constexpr const char *kHealthResponseFields[] = {"ok", "wifiConnected", "ip", "uptimeMs", "timeAuthority", "timeSource", "timeConfidence", "filesystemReady", "assetContractReady"};
static constexpr const char *kMetaResponseFields[] = {"ok", "apiVersion", "stateVersion", "storageSchemaVersion", "timeAuthority", "timeSource", "timeConfidence"};
static constexpr const char *kContractResponseFields[] = {"ok", "contract"};
static constexpr const char *kStorageSemanticsResponseFields[] = {"ok", "apiVersion", "objects", "backendCapabilities"};

static constexpr const char *kHealthRequired[] = {"ok", "wifiConnected", "ip", "uptimeMs", "timeAuthority", "timeSource", "timeConfidence", "filesystemReady", "assetContractReady"};
static constexpr const char *kHealthSections[] = {"healthStatus"};
static constexpr const char *kMetaRequired[] = {"ok", "apiVersion", "stateVersion", "storageSchemaVersion", "timeAuthority", "timeSource", "timeConfidence"};
static constexpr const char *kMetaSections[] = {"versions", "storageRuntime", "assetContract", "runtimeEvents", "platformSupport", "deviceIdentity", "capabilities", "stateSurfaces", "releaseValidation"};
static constexpr const char *kContractDocumentRequired[] = {"ok", "contract"};
static constexpr const char *kContractDocumentSections[] = {"contract"};
static constexpr const char *kStorageSemanticsRequired[] = {"ok", "apiVersion", "objects", "backendCapabilities"};
static constexpr const char *kStorageSemanticsSections[] = {"storageSemantics"};
static constexpr const char *kReleaseValidationRequired[] = {"candidateBundleKind", "verifiedBundleKind", "hostReportType", "deviceReportType", "validationSchemaVersion"};
static constexpr const char *kReleaseValidationSections[] = {"releaseValidation"};

static constexpr WebRouteOperationCatalogEntry kHealthOps[] = {
    {"GET", "api", "health", nullptr, 0U, kHealthResponseFields, sizeof(kHealthResponseFields) / sizeof(kHealthResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kContractOps[] = {
    {"GET", "api", "contractDocument", nullptr, 0U, kContractResponseFields, sizeof(kContractResponseFields) / sizeof(kContractResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kMetaOps[] = {
    {"GET", "api", "meta", nullptr, 0U, kMetaResponseFields, sizeof(kMetaResponseFields) / sizeof(kMetaResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kStorageSemanticsOps[] = {
    {"GET", "api", "storageSemantics", nullptr, 0U, kStorageSemanticsResponseFields, sizeof(kStorageSemanticsResponseFields) / sizeof(kStorageSemanticsResponseFields[0])},
};

static constexpr WebRouteCatalogEntry kCoreRoutes[] = {
    {"health", WEB_ROUTE_HEALTH, "diagnostic", "web_health", "rich_console", "stable", "GET", kHealthOps, sizeof(kHealthOps) / sizeof(kHealthOps[0])},
    {"contract", WEB_ROUTE_CONTRACT, "tooling", "web_contract", "rich_console", "stable", "GET", kContractOps, sizeof(kContractOps) / sizeof(kContractOps[0])},
    {"meta", WEB_ROUTE_META, "diagnostic", "system_runtime_service_init", "rich_console", "stable", "GET", kMetaOps, sizeof(kMetaOps) / sizeof(kMetaOps[0])},
    {"storageSemantics", WEB_ROUTE_STORAGE_SEMANTICS, "diagnostic", "storage_semantics", "rich_console", "stable", "GET", kStorageSemanticsOps, sizeof(kStorageSemanticsOps) / sizeof(kStorageSemanticsOps[0])},
};

static constexpr WebApiSchemaCatalogEntry kCoreApiSchemas[] = {
    {"health", kHealthRequired, sizeof(kHealthRequired) / sizeof(kHealthRequired[0]), kHealthSections, sizeof(kHealthSections) / sizeof(kHealthSections[0])},
    {"meta", kMetaRequired, sizeof(kMetaRequired) / sizeof(kMetaRequired[0]), kMetaSections, sizeof(kMetaSections) / sizeof(kMetaSections[0])},
    {"contractDocument", kContractDocumentRequired, sizeof(kContractDocumentRequired) / sizeof(kContractDocumentRequired[0]), kContractDocumentSections, sizeof(kContractDocumentSections) / sizeof(kContractDocumentSections[0])},
    {"storageSemantics", kStorageSemanticsRequired, sizeof(kStorageSemanticsRequired) / sizeof(kStorageSemanticsRequired[0]), kStorageSemanticsSections, sizeof(kStorageSemanticsSections) / sizeof(kStorageSemanticsSections[0])},
    {"releaseValidation", kReleaseValidationRequired, sizeof(kReleaseValidationRequired) / sizeof(kReleaseValidationRequired[0]), kReleaseValidationSections, sizeof(kReleaseValidationSections) / sizeof(kReleaseValidationSections[0])},
};
}

void web_register_route_module_core(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_HEALTH, HTTP_GET, handle_health_request);
    server.on(WEB_ROUTE_CONTRACT, HTTP_GET, handle_contract_request);
    server.on(WEB_ROUTE_META, HTTP_GET, handle_meta_request);
    server.on(WEB_ROUTE_STORAGE_SEMANTICS, HTTP_GET, handle_storage_semantics_request);
}

void web_install_route_module_core(void)
{
    (void)web_route_module_register("core", web_register_route_module_core,
                                    kCoreRoutes, sizeof(kCoreRoutes) / sizeof(kCoreRoutes[0]),
                                    kCoreApiSchemas, sizeof(kCoreApiSchemas) / sizeof(kCoreApiSchemas[0]),
                                    nullptr, 0U);
}

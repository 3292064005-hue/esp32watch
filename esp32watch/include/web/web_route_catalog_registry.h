#ifndef WEB_ROUTE_CATALOG_REGISTRY_H
#define WEB_ROUTE_CATALOG_REGISTRY_H

#include <stddef.h>
#include "web/web_route_catalog.h"

size_t web_route_catalog_count(void);
const WebRouteCatalogEntry *web_route_catalog_at(size_t index);
const WebRouteCatalogEntry *web_route_catalog_find(const char *route_key);
const WebRouteOperationCatalogEntry *web_route_catalog_default_operation(const WebRouteCatalogEntry *route);
const WebRouteOperationCatalogEntry *web_route_catalog_operation_for_method(const WebRouteCatalogEntry *route, const char *method);

size_t web_api_schema_catalog_count(void);
const WebApiSchemaCatalogEntry *web_api_schema_catalog_at(size_t index);
const WebApiSchemaCatalogEntry *web_api_schema_catalog_find(const char *name);

size_t web_state_schema_catalog_count(void);
const WebStateSchemaCatalogEntry *web_state_schema_catalog_at(size_t index);
const WebStateSchemaCatalogEntry *web_state_schema_catalog_find(const char *name);

#endif

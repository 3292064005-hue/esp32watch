#ifndef WEB_ROUTE_MODULE_MANIFEST_H
#define WEB_ROUTE_MODULE_MANIFEST_H

#include <stddef.h>

#define WEB_ROUTE_MODULE_MANIFEST_PROVIDER_COUNT 7U

void web_route_module_manifest_register_providers(void);
size_t web_route_module_manifest_provider_count(void);

#endif

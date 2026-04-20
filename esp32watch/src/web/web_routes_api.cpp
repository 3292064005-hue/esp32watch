#include "web/web_route_module_registry.h"

void web_register_api_routes(AsyncWebServer &server)
{
    for (size_t i = 0U; i < web_route_module_count(); ++i) {
        const WebRouteModuleDescriptor *module = web_route_module_at(i);
        if (module != nullptr && module->register_fn != nullptr) {
            module->register_fn(server);
        }
    }
}

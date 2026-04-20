#ifndef WEB_ROUTE_MODULE_REGISTRY_H
#define WEB_ROUTE_MODULE_REGISTRY_H

#include <stddef.h>
#include <ESPAsyncWebServer.h>

struct WebRouteModuleDescriptor {
    const char *name;
    void (*register_fn)(AsyncWebServer &server);
};

void web_route_module_registry_reset(void);
bool web_route_module_register(const char *name, void (*register_fn)(AsyncWebServer &server));
typedef void (*WebRouteModuleInstaller)(void);

bool web_route_module_register_provider(const char *name, WebRouteModuleInstaller installer);
void web_route_module_registry_ensure_initialized(void);
size_t web_route_module_count(void);
const WebRouteModuleDescriptor *web_route_module_at(size_t index);

void web_install_route_module_core(void);
void web_install_route_module_actions(void);
void web_install_route_module_config(void);
void web_install_route_module_reset(void);
void web_install_route_module_state(void);
void web_install_route_module_display(void);
void web_install_route_module_input(void);

void web_register_route_module_core(AsyncWebServer &server);
void web_register_route_module_actions(AsyncWebServer &server);
void web_register_route_module_config(AsyncWebServer &server);
void web_register_route_module_reset(AsyncWebServer &server);
void web_register_route_module_state(AsyncWebServer &server);
void web_register_route_module_display(AsyncWebServer &server);
void web_register_route_module_input(AsyncWebServer &server);

#endif

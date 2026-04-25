#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

namespace {
static constexpr const char *kDisplayFrameResponseFields[] = {"ok", "width", "height", "presentCount", "bufferHex"};
static constexpr const char *kTrackedActionResponseFields[] = {"ok", "actionId", "requestId", "actionType", "trackPath", "queueDepth"};
static constexpr const char *kDisplayFrameRequired[] = {"ok", "width", "height", "presentCount", "bufferHex"};
static constexpr const char *kDisplayFrameSections[] = {"displayFrame"};

static constexpr WebRouteOperationCatalogEntry kDisplayFrameOps[] = {
    {"GET", "api", "displayFrame", nullptr, 0U, kDisplayFrameResponseFields, sizeof(kDisplayFrameResponseFields) / sizeof(kDisplayFrameResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kDisplayOverlayOps[] = {
    {"POST", "api", "trackedActionAccepted", nullptr, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kDisplayOverlayClearOps[] = {
    {"POST", "api", "trackedActionAccepted", nullptr, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
};
static constexpr WebRouteCatalogEntry kDisplayRoutes[] = {
    {"displayFrame", WEB_ROUTE_DISPLAY_FRAME, "console", "display_service", "rich_console", "stable", "GET", kDisplayFrameOps, sizeof(kDisplayFrameOps) / sizeof(kDisplayFrameOps[0])},
    {"displayOverlay", WEB_ROUTE_DISPLAY_OVERLAY, "control", "web_overlay", "rich_console", "stable", "POST", kDisplayOverlayOps, sizeof(kDisplayOverlayOps) / sizeof(kDisplayOverlayOps[0])},
    {"displayOverlayClear", WEB_ROUTE_DISPLAY_OVERLAY_CLEAR, "control", "web_overlay", "rich_console", "stable", "POST", kDisplayOverlayClearOps, sizeof(kDisplayOverlayClearOps) / sizeof(kDisplayOverlayClearOps[0])},
};
static constexpr WebApiSchemaCatalogEntry kDisplayApiSchemas[] = {
    {"displayFrame", kDisplayFrameRequired, sizeof(kDisplayFrameRequired) / sizeof(kDisplayFrameRequired[0]), kDisplayFrameSections, sizeof(kDisplayFrameSections) / sizeof(kDisplayFrameSections[0])},
};
}

void web_register_route_module_display(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_DISPLAY_FRAME, HTTP_GET, handle_display_frame_request);
    server.on(WEB_ROUTE_DISPLAY_OVERLAY, HTTP_POST, handle_display_overlay_request, nullptr, capture_request_body);
    server.on(WEB_ROUTE_DISPLAY_OVERLAY_CLEAR, HTTP_POST, handle_display_overlay_clear_request, nullptr, capture_request_body);
}

void web_install_route_module_display(void)
{
    (void)web_route_module_register("display", web_register_route_module_display,
                                    kDisplayRoutes, sizeof(kDisplayRoutes) / sizeof(kDisplayRoutes[0]),
                                    kDisplayApiSchemas, sizeof(kDisplayApiSchemas) / sizeof(kDisplayApiSchemas[0]),
                                    nullptr, 0U);
}

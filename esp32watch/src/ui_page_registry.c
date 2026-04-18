#include "ui_page_registry.h"
#include "ui_page_catalog.h"

static const UiPageOps g_null_page = {
    .title = "N/A",
    .refresh_policy = UI_REFRESH_NONE,
    .allow_auto_sleep = true,
    .allow_popup = true,
    .nav_group = UI_NAV_GROUP_ROOT,
    .presenter = UI_PRESENTER_NONE,
    .render = 0,
    .handle = 0,
};

const UiPageOps *ui_page_registry_get(PageId page)
{
    const UiPageDescriptor *descriptor = ui_page_catalog_find(page);

    if (descriptor == NULL) {
        return &g_null_page;
    }
    if (descriptor->ops.render == 0 && descriptor->ops.handle == 0) {
        return &g_null_page;
    }
    return &descriptor->ops;
}

const char *ui_page_registry_title(PageId page)
{
    return ui_page_registry_get(page)->title;
}

UiPageRefreshPolicy ui_page_registry_refresh_policy(PageId page)
{
    return ui_page_registry_get(page)->refresh_policy;
}

bool ui_page_registry_allows_auto_sleep(PageId page)
{
    return ui_page_registry_get(page)->allow_auto_sleep;
}

bool ui_page_registry_allows_popup(PageId page)
{
    return ui_page_registry_get(page)->allow_popup;
}

uint8_t ui_page_registry_nav_group(PageId page)
{
    return ui_page_registry_get(page)->nav_group;
}

UiPagePresenterId ui_page_registry_presenter(PageId page)
{
    return (UiPagePresenterId)ui_page_registry_get(page)->presenter;
}

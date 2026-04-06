#ifndef UI_PAGE_REGISTRY_H
#define UI_PAGE_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

const UiPageOps *ui_page_registry_get(PageId page);
const char *ui_page_registry_title(PageId page);
UiPageRefreshPolicy ui_page_registry_refresh_policy(PageId page);
bool ui_page_registry_allows_auto_sleep(PageId page);
bool ui_page_registry_allows_popup(PageId page);
uint8_t ui_page_registry_nav_group(PageId page);
UiPagePresenterId ui_page_registry_presenter(PageId page);

#ifdef __cplusplus
}
#endif

#endif

#include "ui_page_catalog.h"
#include "ui_page_module_registry.h"

uint8_t ui_page_catalog_count(void)
{
    return (uint8_t)ui_page_registry_catalog_count();
}

bool ui_page_catalog_get(uint8_t index, UiPageDescriptor *out)
{
    const UiPageDescriptor *descriptor;
    if (out == NULL) {
        return false;
    }
    descriptor = ui_page_registry_catalog_at(index);
    if (descriptor == NULL) {
        return false;
    }
    *out = *descriptor;
    return true;
}

const UiPageDescriptor *ui_page_catalog_find(PageId page)
{
    return ui_page_registry_find_descriptor(page);
}

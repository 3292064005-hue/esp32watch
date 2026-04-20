#ifndef UI_PAGE_MODULE_REGISTRY_H
#define UI_PAGE_MODULE_REGISTRY_H

#include <stddef.h>
#include "ui_page_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    const UiPageDescriptor *pages;
    size_t page_count;
} UiPageModuleDescriptor;

void ui_page_module_registry_reset(void);
bool ui_page_module_register(const char *name, const UiPageDescriptor *pages, size_t page_count);
typedef void (*UiPageModuleInstaller)(void);

bool ui_page_module_register_provider(const char *name, UiPageModuleInstaller installer);
void ui_page_module_registry_ensure_initialized(void);

size_t ui_page_module_count(void);
const UiPageModuleDescriptor *ui_page_module_at(size_t index);
size_t ui_page_registry_catalog_count(void);
const UiPageDescriptor *ui_page_registry_catalog_at(size_t index);
const UiPageDescriptor *ui_page_registry_find_descriptor(PageId page);

void ui_page_register_core_module(void);

#ifdef __cplusplus
}
#endif

#endif

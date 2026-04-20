#include "ui_page_module_registry.h"
#include "ui_page_module_manifest.h"
#include <stdlib.h>
#include <string.h>

typedef struct UiPageModuleNode {
    UiPageModuleDescriptor module;
    struct UiPageModuleNode *next;
} UiPageModuleNode;

typedef struct UiPageModuleProviderNode {
    const char *name;
    UiPageModuleInstaller installer;
    struct UiPageModuleProviderNode *next;
} UiPageModuleProviderNode;

static UiPageModuleNode *g_module_head;
static UiPageModuleNode *g_module_tail;
static size_t g_module_count;
static UiPageModuleProviderNode *g_provider_head;
static UiPageModuleProviderNode *g_provider_tail;
static bool g_registry_initialized;

static void free_modules(void)
{
    UiPageModuleNode *node = g_module_head;
    while (node != NULL) {
        UiPageModuleNode *next = node->next;
        free(node);
        node = next;
    }
    g_module_head = NULL;
    g_module_tail = NULL;
    g_module_count = 0U;
}

void ui_page_module_registry_reset(void)
{
    free_modules();
    g_registry_initialized = false;
}

bool ui_page_module_register(const char *name, const UiPageDescriptor *pages, size_t page_count)
{
    UiPageModuleNode *node;
    if (name == NULL || pages == NULL || page_count == 0U) {
        return false;
    }
    node = (UiPageModuleNode *)calloc(1U, sizeof(*node));
    if (node == NULL) {
        return false;
    }
    node->module.name = name;
    node->module.pages = pages;
    node->module.page_count = page_count;
    if (g_module_tail != NULL) {
        g_module_tail->next = node;
    } else {
        g_module_head = node;
    }
    g_module_tail = node;
    ++g_module_count;
    return true;
}

bool ui_page_module_register_provider(const char *name, UiPageModuleInstaller installer)
{
    UiPageModuleProviderNode *node;
    UiPageModuleProviderNode *cursor;
    if (name == NULL || name[0] == '\0' || installer == NULL) {
        return false;
    }
    for (cursor = g_provider_head; cursor != NULL; cursor = cursor->next) {
        if (cursor->installer == installer || strcmp(cursor->name, name) == 0) {
            return true;
        }
    }
    node = (UiPageModuleProviderNode *)calloc(1U, sizeof(*node));
    if (node == NULL) {
        return false;
    }
    node->name = name;
    node->installer = installer;
    if (g_provider_tail != NULL) {
        g_provider_tail->next = node;
    } else {
        g_provider_head = node;
    }
    g_provider_tail = node;
    return true;
}

void ui_page_module_registry_ensure_initialized(void)
{
    UiPageModuleProviderNode *provider;
    if (g_registry_initialized) {
        return;
    }
    g_registry_initialized = true;
    ui_page_module_manifest_register_providers();
    for (provider = g_provider_head; provider != NULL; provider = provider->next) {
        provider->installer();
    }
}

size_t ui_page_module_count(void)
{
    ui_page_module_registry_ensure_initialized();
    return g_module_count;
}

const UiPageModuleDescriptor *ui_page_module_at(size_t index)
{
    size_t current = 0U;
    UiPageModuleNode *node;
    ui_page_module_registry_ensure_initialized();
    for (node = g_module_head; node != NULL; node = node->next) {
        if (current == index) {
            return &node->module;
        }
        ++current;
    }
    return NULL;
}

size_t ui_page_registry_catalog_count(void)
{
    size_t count = 0U;
    UiPageModuleNode *node;
    ui_page_module_registry_ensure_initialized();
    for (node = g_module_head; node != NULL; node = node->next) {
        count += node->module.page_count;
    }
    return count;
}

const UiPageDescriptor *ui_page_registry_catalog_at(size_t index)
{
    size_t remaining = index;
    UiPageModuleNode *node;
    ui_page_module_registry_ensure_initialized();
    for (node = g_module_head; node != NULL; node = node->next) {
        if (remaining < node->module.page_count) {
            return &node->module.pages[remaining];
        }
        remaining -= node->module.page_count;
    }
    return NULL;
}

const UiPageDescriptor *ui_page_registry_find_descriptor(PageId page)
{
    size_t i;
    ui_page_module_registry_ensure_initialized();
    for (i = 0U; i < ui_page_registry_catalog_count(); ++i) {
        const UiPageDescriptor *descriptor = ui_page_registry_catalog_at(i);
        if (descriptor != NULL && descriptor->page == page) {
            return descriptor;
        }
    }
    return NULL;
}

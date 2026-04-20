#include "app_command_registry.h"
#include "app_command_module_manifest.h"
#include <stdlib.h>
#include <string.h>

typedef struct AppCommandCatalogBlockNode {
    AppCommandCatalogBlock block;
    struct AppCommandCatalogBlockNode *next;
} AppCommandCatalogBlockNode;

typedef struct AppCommandCompanionBindingNode {
    AppCommandCompanionBinding binding;
    struct AppCommandCompanionBindingNode *next;
} AppCommandCompanionBindingNode;

typedef struct AppCommandModuleProviderNode {
    const char *name;
    AppCommandModuleInstaller installer;
    struct AppCommandModuleProviderNode *next;
} AppCommandModuleProviderNode;

static AppCommandCatalogBlockNode *g_block_head;
static AppCommandCatalogBlockNode *g_block_tail;
static size_t g_block_count;
static AppCommandCompanionBindingNode *g_binding_head;
static AppCommandCompanionBindingNode *g_binding_tail;
static size_t g_companion_binding_count;
static AppCommandModuleProviderNode *g_provider_head;
static AppCommandModuleProviderNode *g_provider_tail;
static bool g_registry_initialized;

static void free_blocks(void)
{
    AppCommandCatalogBlockNode *node = g_block_head;
    while (node != NULL) {
        AppCommandCatalogBlockNode *next = node->next;
        free(node);
        node = next;
    }
    g_block_head = NULL;
    g_block_tail = NULL;
    g_block_count = 0U;
}

static void free_bindings(void)
{
    AppCommandCompanionBindingNode *node = g_binding_head;
    while (node != NULL) {
        AppCommandCompanionBindingNode *next = node->next;
        free(node);
        node = next;
    }
    g_binding_head = NULL;
    g_binding_tail = NULL;
    g_companion_binding_count = 0U;
}

void app_command_registry_reset(void)
{
    free_blocks();
    free_bindings();
    g_registry_initialized = false;
}

bool app_command_registry_register_block(const char *name, const AppCommandDescriptor *commands, size_t command_count)
{
    AppCommandCatalogBlockNode *node;
    if (name == NULL || commands == NULL || command_count == 0U) {
        return false;
    }
    node = (AppCommandCatalogBlockNode *)calloc(1U, sizeof(*node));
    if (node == NULL) {
        return false;
    }
    node->block.name = name;
    node->block.commands = commands;
    node->block.command_count = command_count;
    if (g_block_tail != NULL) {
        g_block_tail->next = node;
    } else {
        g_block_head = node;
    }
    g_block_tail = node;
    ++g_block_count;
    return true;
}

bool app_command_registry_register_companion_binding(const char *key, AppCommandType type)
{
    AppCommandCompanionBindingNode *node;
    if (key == NULL || key[0] == '\0') {
        return false;
    }
    node = (AppCommandCompanionBindingNode *)calloc(1U, sizeof(*node));
    if (node == NULL) {
        return false;
    }
    node->binding.key = key;
    node->binding.type = type;
    if (g_binding_tail != NULL) {
        g_binding_tail->next = node;
    } else {
        g_binding_head = node;
    }
    g_binding_tail = node;
    ++g_companion_binding_count;
    return true;
}

bool app_command_registry_register_provider(const char *name, AppCommandModuleInstaller installer)
{
    AppCommandModuleProviderNode *node;
    AppCommandModuleProviderNode *cursor;
    if (name == NULL || name[0] == '\0' || installer == NULL) {
        return false;
    }
    for (cursor = g_provider_head; cursor != NULL; cursor = cursor->next) {
        if (cursor->installer == installer || strcmp(cursor->name, name) == 0) {
            return true;
        }
    }
    node = (AppCommandModuleProviderNode *)calloc(1U, sizeof(*node));
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

void app_command_registry_ensure_initialized(void)
{
    AppCommandModuleProviderNode *provider;
    if (g_registry_initialized) {
        return;
    }
    g_registry_initialized = true;
    app_command_module_manifest_register_providers();
    for (provider = g_provider_head; provider != NULL; provider = provider->next) {
        provider->installer();
    }
}

size_t app_command_registry_block_count(void)
{
    app_command_registry_ensure_initialized();
    return g_block_count;
}

const AppCommandCatalogBlock *app_command_registry_block_at(size_t index)
{
    size_t current = 0U;
    AppCommandCatalogBlockNode *node;
    app_command_registry_ensure_initialized();
    node = g_block_head;
    while (node != NULL) {
        if (current == index) {
            return &node->block;
        }
        node = node->next;
        ++current;
    }
    return NULL;
}

const AppCommandDescriptor *app_command_registry_find(AppCommandType type)
{
    AppCommandCatalogBlockNode *node;
    app_command_registry_ensure_initialized();
    for (node = g_block_head; node != NULL; node = node->next) {
        size_t command_index;
        for (command_index = 0U; command_index < node->block.command_count; ++command_index) {
            if (node->block.commands[command_index].type == type) {
                return &node->block.commands[command_index];
            }
        }
    }
    return NULL;
}

const AppCommandDescriptor *app_command_registry_find_by_name(const char *wire_name)
{
    AppCommandCatalogBlockNode *node;
    if (wire_name == NULL || wire_name[0] == '\0') {
        return NULL;
    }
    if (strcmp(wire_name, "restoreDefaults") == 0) {
        return app_command_registry_find(APP_COMMAND_RESET_APP_STATE);
    }
    app_command_registry_ensure_initialized();
    for (node = g_block_head; node != NULL; node = node->next) {
        size_t command_index;
        for (command_index = 0U; command_index < node->block.command_count; ++command_index) {
            if (strcmp(node->block.commands[command_index].wire_name, wire_name) == 0) {
                return &node->block.commands[command_index];
            }
        }
    }
    return NULL;
}

bool app_command_registry_find_type_for_companion_key(const char *companion_key, AppCommandType *out_type)
{
    AppCommandCompanionBindingNode *node;
    if (companion_key == NULL || companion_key[0] == '\0' || out_type == NULL) {
        return false;
    }
    app_command_registry_ensure_initialized();
    for (node = g_binding_head; node != NULL; node = node->next) {
        if (strcmp(node->binding.key, companion_key) == 0) {
            *out_type = node->binding.type;
            return true;
        }
    }
    return false;
}

size_t app_command_registry_catalog_count(void)
{
    size_t count = 0U;
    AppCommandCatalogBlockNode *node;
    app_command_registry_ensure_initialized();
    for (node = g_block_head; node != NULL; node = node->next) {
        count += node->block.command_count;
    }
    return count;
}

const AppCommandDescriptor *app_command_registry_catalog_at(size_t index)
{
    size_t remaining = index;
    AppCommandCatalogBlockNode *node;
    app_command_registry_ensure_initialized();
    for (node = g_block_head; node != NULL; node = node->next) {
        if (remaining < node->block.command_count) {
            return &node->block.commands[remaining];
        }
        remaining -= node->block.command_count;
    }
    return NULL;
}

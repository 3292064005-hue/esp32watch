#ifndef APP_COMMAND_REGISTRY_H
#define APP_COMMAND_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include "app_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    const AppCommandDescriptor *commands;
    size_t command_count;
} AppCommandCatalogBlock;

typedef struct {
    const char *key;
    AppCommandType type;
} AppCommandCompanionBinding;

void app_command_registry_reset(void);
bool app_command_registry_register_block(const char *name, const AppCommandDescriptor *commands, size_t command_count);
bool app_command_registry_register_companion_binding(const char *key, AppCommandType type);
typedef void (*AppCommandModuleInstaller)(void);

bool app_command_registry_register_provider(const char *name, AppCommandModuleInstaller installer);
void app_command_registry_ensure_initialized(void);

size_t app_command_registry_block_count(void);
const AppCommandCatalogBlock *app_command_registry_block_at(size_t index);
const AppCommandDescriptor *app_command_registry_find(AppCommandType type);
const AppCommandDescriptor *app_command_registry_find_by_name(const char *wire_name);
bool app_command_registry_find_type_for_companion_key(const char *companion_key, AppCommandType *out_type);
size_t app_command_registry_catalog_count(void);
const AppCommandDescriptor *app_command_registry_catalog_at(size_t index);

void app_command_register_core_module(void);

#ifdef __cplusplus
}
#endif

#endif

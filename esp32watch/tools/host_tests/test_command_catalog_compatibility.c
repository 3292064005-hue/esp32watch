#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_command.h"

int main(void)
{
    const AppCommandDescriptor *canonical = app_command_describe(APP_COMMAND_RESET_APP_STATE);
    const AppCommandDescriptor *legacy = app_command_describe_by_name("restoreDefaults");
    const AppCommandDescriptor *legacy_catalog = app_command_describe(APP_COMMAND_RESTORE_DEFAULTS);

    assert(canonical != NULL);
    assert(strcmp(canonical->wire_name, "resetAppState") == 0);
    assert(legacy == canonical);
    assert(legacy_catalog != NULL);
    assert(strcmp(legacy_catalog->wire_name, "restoreDefaults") == 0);
    assert(legacy_catalog->web_exposed == false);
    puts("[OK] command catalog compatibility behavior check passed");
    return 0;
}

#include <assert.h>
#include <stdio.h>

#include "board_manifest.h"
#include "platform_api.h"

PlatformGpioPort g_platform_gpioa = {0};
PlatformGpioPort g_platform_gpiob = {1};

int main(void)
{
    const BoardManifest *manifest = board_manifest_get();
    assert(manifest != NULL);
    assert(board_manifest_keypad_mapping_valid());
    assert(board_manifest_gpio_contract_valid());
    assert(board_manifest_profile_name() != NULL);
    puts("[OK] board profile matrix check passed");
    return 0;
}

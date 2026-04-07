#include "power.h"

void power_init(void)
{
}

uint8_t power_brightness_to_contrast(uint8_t level)
{
    static const uint8_t lut[] = {24, 72, 144, 220};
    if (level > 3U) {
        level = 3U;
    }
    return lut[level];
}

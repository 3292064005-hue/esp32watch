#ifndef POWER_H
#define POWER_H

#include <stdint.h>

void power_init(void);
uint8_t power_brightness_to_contrast(uint8_t level);

#endif

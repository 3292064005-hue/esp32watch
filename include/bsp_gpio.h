#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void bsp_gpio_init_output_pp(PlatformGpioPort *port, uint16_t pin, uint32_t speed);
void bsp_gpio_init_output_od(PlatformGpioPort *port, uint16_t pin, uint32_t speed);
void bsp_gpio_init_input_pullup(PlatformGpioPort *port, uint16_t pin);
void bsp_gpio_write(PlatformGpioPort *port, uint16_t pin, bool high);
bool bsp_gpio_read(PlatformGpioPort *port, uint16_t pin);

#ifdef __cplusplus
}
#endif

#endif

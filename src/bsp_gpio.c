#include "bsp_gpio.h"

static void bsp_gpio_init_mode(PlatformGpioPort *port, uint16_t pin, uint32_t mode, uint32_t pull, uint32_t speed)
{
    PlatformGpioConfig gpio = {0};
    if (port == NULL || pin == 0U) {
        return;
    }
    gpio.pin_mask = pin;
    gpio.mode = mode;
    gpio.pull = pull;
    gpio.speed = speed;
    platform_gpio_init(port, &gpio);
}

void bsp_gpio_init_output_pp(PlatformGpioPort *port, uint16_t pin, uint32_t speed)
{
    bsp_gpio_init_mode(port, pin, PLATFORM_GPIO_MODE_OUTPUT_PP, PLATFORM_GPIO_NO_PULL, speed);
}

void bsp_gpio_init_output_od(PlatformGpioPort *port, uint16_t pin, uint32_t speed)
{
    bsp_gpio_init_mode(port, pin, PLATFORM_GPIO_MODE_OUTPUT_OD, PLATFORM_GPIO_NO_PULL, speed);
}

void bsp_gpio_init_input_pullup(PlatformGpioPort *port, uint16_t pin)
{
    bsp_gpio_init_mode(port, pin, PLATFORM_GPIO_MODE_INPUT, PLATFORM_GPIO_PULL_UP, PLATFORM_GPIO_SPEED_LOW);
}

void bsp_gpio_write(PlatformGpioPort *port, uint16_t pin, bool high)
{
    if (port == NULL || pin == 0U) {
        return;
    }
    platform_gpio_write(port, pin, high ? PLATFORM_PIN_HIGH : PLATFORM_PIN_LOW);
}

bool bsp_gpio_read(PlatformGpioPort *port, uint16_t pin)
{
    if (port == NULL || pin == 0U) {
        return false;
    }
    return platform_gpio_read(port, pin) == PLATFORM_PIN_HIGH;
}

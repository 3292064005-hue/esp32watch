#ifndef DRV_SSD1306_H
#define DRV_SSD1306_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

PlatformStatus drv_ssd1306_write_cmds(const uint8_t *cmds, uint8_t count);
PlatformStatus drv_ssd1306_write_data(const uint8_t *data, uint8_t len);
PlatformStatus drv_ssd1306_init_panel(void);
PlatformStatus drv_ssd1306_set_contrast(uint8_t value);
PlatformStatus drv_ssd1306_set_sleep(bool sleep);

#ifdef __cplusplus
}
#endif

#endif


#ifndef DRV_VIBRATOR_H
#define DRV_VIBRATOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void drv_vibrator_init(void);
void drv_vibrator_set(bool enabled);
bool drv_vibrator_is_active(void);

#ifdef __cplusplus
}
#endif

#endif

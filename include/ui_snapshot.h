#ifndef UI_SNAPSHOT_H
#define UI_SNAPSHOT_H

#include "ui_internal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build a consolidated UI system snapshot from model and runtime services.
 *
 * @param[out] out Snapshot destination. Must not be NULL.
 * @param[in] now_ms Monotonic tick used for age/offline calculations.
 * @return void
 * @throws None.
 */
void ui_snapshot_build(UiSystemSnapshot *out, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

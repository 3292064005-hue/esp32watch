#ifndef UI_APP_CATALOG_H
#define UI_APP_CATALOG_H

#include <stddef.h>
#include <stdbool.h>
#include "ui_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*UiAppStatusComposeFn)(char *out, size_t out_size);

typedef struct {
    const char *name;
    PageId page;
    bool accent;
    UiAppStatusComposeFn compose_status;
} UiAppDescriptor;

/**
 * @brief Return the total number of apps exposed through the Apps menu.
 *
 * @param None.
 * @return Number of catalog entries.
 * @throws None.
 */
uint8_t ui_app_catalog_count(void);

/**
 * @brief Copy one Apps-menu descriptor from the static manifest.
 *
 * @param[in] index Zero-based app catalog index.
 * @param[out] out Destination descriptor buffer.
 * @return true when @p out was populated; false when the index is out of range or @p out is NULL.
 * @throws None.
 * @boundary_behavior Returns false without modifying state when @p index is invalid.
 */
bool ui_app_catalog_get(uint8_t index, UiAppDescriptor *out);

#ifdef __cplusplus
}
#endif

#endif

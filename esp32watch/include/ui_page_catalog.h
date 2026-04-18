#ifndef UI_PAGE_CATALOG_H
#define UI_PAGE_CATALOG_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PageId page;
    UiPageOps ops;
} UiPageDescriptor;

/**
 * @brief Return the number of statically registered UI pages.
 *
 * @return Count of descriptors exposed by the static page manifest.
 * @throws None.
 */
uint8_t ui_page_catalog_count(void);

/**
 * @brief Copy a page descriptor from the static page manifest.
 *
 * @param[in] index Zero-based descriptor index.
 * @param[out] out Destination descriptor.
 * @return true when the descriptor exists and @p out was populated.
 * @throws None.
 * @boundary_behavior Returns false when @p out is NULL or @p index is out of range.
 */
bool ui_page_catalog_get(uint8_t index, UiPageDescriptor *out);

/**
 * @brief Resolve a page descriptor by page id.
 *
 * @param[in] page Requested page id.
 * @return Descriptor pointer when the page is registered; NULL otherwise.
 * @throws None.
 */
const UiPageDescriptor *ui_page_catalog_find(PageId page);

#ifdef __cplusplus
}
#endif

#endif

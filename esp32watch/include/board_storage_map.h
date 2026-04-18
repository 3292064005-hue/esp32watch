#ifndef BOARD_STORAGE_MAP_H
#define BOARD_STORAGE_MAP_H

#include <stdint.h>

/*
 * Active target contracts are profile-scoped. ESP32 builds derive storage layout
 * from the partition contract instead of inheriting the legacy STM32 flash-page
 * reservation map.
 */
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
#include "esp32_partition_contract.h"
#define FLASH_STORAGE_PAGE_SIZE           0UL
#define FLASH_STORAGE_PAGE0_ADDRESS       0UL
#define FLASH_STORAGE_PAGE1_ADDRESS       0UL
#define FLASH_STORAGE_RESERVED_BYTES      0UL
#define FLASH_STORAGE_REGION_END          0UL
#define FLASH_STORAGE_REGION_START        0UL
#define FLASH_STORAGE_EXPECTED_FLASH_END  0UL
#else
#define FLASH_STORAGE_PAGE_SIZE           1024UL
#define FLASH_STORAGE_PAGE0_ADDRESS       0x0800F800UL
#define FLASH_STORAGE_PAGE1_ADDRESS       0x0800FC00UL
#define FLASH_STORAGE_RESERVED_BYTES      (FLASH_STORAGE_PAGE_SIZE * 2UL)
#define FLASH_STORAGE_REGION_END          (FLASH_STORAGE_PAGE1_ADDRESS + FLASH_STORAGE_PAGE_SIZE)
#define FLASH_STORAGE_REGION_START        FLASH_STORAGE_PAGE0_ADDRESS
#define FLASH_STORAGE_EXPECTED_FLASH_END  0x08010000UL

_Static_assert((FLASH_STORAGE_PAGE0_ADDRESS % FLASH_STORAGE_PAGE_SIZE) == 0UL, "FLASH storage page0 must be page aligned");
_Static_assert((FLASH_STORAGE_PAGE1_ADDRESS % FLASH_STORAGE_PAGE_SIZE) == 0UL, "FLASH storage page1 must be page aligned");
_Static_assert(FLASH_STORAGE_PAGE1_ADDRESS == (FLASH_STORAGE_PAGE0_ADDRESS + FLASH_STORAGE_PAGE_SIZE), "FLASH storage pages must be contiguous");
_Static_assert(FLASH_STORAGE_REGION_END == FLASH_STORAGE_EXPECTED_FLASH_END, "FLASH storage reservation must occupy the final two pages");
#endif

#endif

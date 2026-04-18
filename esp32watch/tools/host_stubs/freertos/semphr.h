#pragma once
#include "freertos/FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef void* SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)0x1; }
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks) { (void)semaphore; (void)ticks; return pdTRUE; }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) { (void)semaphore; return pdTRUE; }
static inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) { (void)semaphore; }

#ifdef __cplusplus
}
#endif

#pragma once
#include "freertos/FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif

static inline BaseType_t xTaskCreatePinnedToCore(void (*task)(void *),
                                                 const char *name,
                                                 uint32_t stack_depth,
                                                 void *arg,
                                                 UBaseType_t priority,
                                                 TaskHandle_t *out_handle,
                                                 BaseType_t core_id) {
  (void)task; (void)name; (void)stack_depth; (void)arg; (void)priority; (void)core_id;
  if (out_handle != 0) {
    *out_handle = (TaskHandle_t)0x1;
  }
  return pdPASS;
}
static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
static inline void vTaskDelete(TaskHandle_t task) { (void)task; }
#ifdef __cplusplus
}
#endif

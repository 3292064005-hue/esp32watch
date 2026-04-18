#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void* TaskHandle_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;

#define pdPASS 1
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xffffffffUL
#define portTICK_PERIOD_MS 1UL

#ifdef __cplusplus
}
#endif

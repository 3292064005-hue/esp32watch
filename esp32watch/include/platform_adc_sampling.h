#ifndef PLATFORM_ADC_SAMPLING_H
#define PLATFORM_ADC_SAMPLING_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

bool platform_adc_sample_trimmed_mean(PlatformAdcDevice *device, uint8_t samples, uint32_t poll_timeout_ms, uint32_t *out_average, uint8_t *out_valid_samples, uint16_t *out_fault_code);

#ifdef __cplusplus
}
#endif

#endif

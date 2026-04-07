#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

bool bsp_adc_sample_trimmed_mean(PlatformAdcDevice *hadc,
                                 uint8_t sample_count,
                                 uint32_t timeout_ms,
                                 uint32_t *out_average,
                                 uint8_t *out_valid_samples,
                                 uint16_t *out_fault_code);

#ifdef __cplusplus
}
#endif

#endif

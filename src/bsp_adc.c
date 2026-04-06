#include "bsp_adc.h"

bool bsp_adc_sample_trimmed_mean(PlatformAdcDevice *hadc,
                                 uint8_t sample_count,
                                 uint32_t timeout_ms,
                                 uint32_t *out_average,
                                 uint8_t *out_valid_samples,
                                 uint16_t *out_fault_code)
{
    uint32_t sum = 0U;
    uint32_t min_v = 0xFFFFFFFFUL;
    uint32_t max_v = 0U;
    uint8_t valid = 0U;

    if (out_average != NULL) {
        *out_average = 0U;
    }
    if (out_valid_samples != NULL) {
        *out_valid_samples = 0U;
    }
    if (out_fault_code != NULL) {
        *out_fault_code = 0U;
    }
    if (hadc == NULL || out_average == NULL || sample_count == 0U) {
        if (out_fault_code != NULL) {
            *out_fault_code = 0xFFFEU;
        }
        return false;
    }

    for (uint8_t i = 0U; i < sample_count; ++i) {
        PlatformStatus st = platform_adc_start(hadc);
        if (st != PLATFORM_STATUS_OK) {
            if (out_fault_code != NULL) {
                *out_fault_code = (uint16_t)st;
            }
            continue;
        }
        st = platform_adc_poll(hadc, timeout_ms);
        if (st == PLATFORM_STATUS_OK) {
            uint32_t value = platform_adc_read(hadc);
            sum += value;
            if (value < min_v) {
                min_v = value;
            }
            if (value > max_v) {
                max_v = value;
            }
            valid++;
        } else if (out_fault_code != NULL) {
            *out_fault_code = (uint16_t)st;
        }
        (void)platform_adc_stop(hadc);
    }

    if (out_valid_samples != NULL) {
        *out_valid_samples = valid;
    }
    if (valid >= 3U) {
        sum -= min_v;
        sum -= max_v;
        *out_average = sum / (uint32_t)(valid - 2U);
        return true;
    }
    if (valid > 0U) {
        *out_average = sum / valid;
        return true;
    }
    if (out_fault_code != NULL && *out_fault_code == 0U) {
        *out_fault_code = 0xFFFFU;
    }
    return false;
}







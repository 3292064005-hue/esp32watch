#include "platform_adc_sampling.h"
#include "app_config.h"

#include <stddef.h>

#if !defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
#include "bsp_adc.h"
#endif

static void sort_values(uint32_t *values, uint8_t count)
{
    for (uint8_t i = 0U; i < count; ++i) {
        for (uint8_t j = (uint8_t)(i + 1U); j < count; ++j) {
            if (values[j] < values[i]) {
                uint32_t tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }
}

bool platform_adc_sample_trimmed_mean(PlatformAdcDevice *device, uint8_t samples, uint32_t poll_timeout_ms, uint32_t *out_average, uint8_t *out_valid_samples, uint16_t *out_fault_code)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    uint32_t values[16] = {0U};
    uint32_t sum = 0U;
    uint8_t count = 0U;

    if (out_average == NULL || out_valid_samples == NULL || out_fault_code == NULL || device == NULL || samples < 3U || samples > 16U) {
        return false;
    }

    *out_average = 0U;
    *out_valid_samples = 0U;
    *out_fault_code = 0U;

    if (platform_adc_init(device) != PLATFORM_STATUS_OK) {
        *out_fault_code = 0x1001U;
        return false;
    }
    if (platform_adc_calibrate(device) != PLATFORM_STATUS_OK) {
        *out_fault_code = 0x1002U;
        return false;
    }

    for (uint8_t i = 0U; i < samples; ++i) {
        if (platform_adc_start(device) != PLATFORM_STATUS_OK) {
            *out_fault_code = 0x1003U;
            return false;
        }
        if (platform_adc_poll(device, poll_timeout_ms) != PLATFORM_STATUS_OK) {
            (void)platform_adc_stop(device);
            *out_fault_code = 0x1004U;
            return false;
        }
        values[count++] = platform_adc_read(device);
        if (platform_adc_stop(device) != PLATFORM_STATUS_OK) {
            *out_fault_code = 0x1005U;
            return false;
        }
    }

    if (count < 3U) {
        *out_fault_code = 0x1006U;
        return false;
    }

    sort_values(values, count);
    for (uint8_t i = 1U; i + 1U < count; ++i) {
        sum += values[i];
    }
    *out_valid_samples = (uint8_t)(count - 2U);
    *out_average = (*out_valid_samples == 0U) ? 0U : (sum / (uint32_t)(*out_valid_samples));
    return *out_valid_samples != 0U;
#else
    return bsp_adc_sample_trimmed_mean(device, samples, poll_timeout_ms, out_average, out_valid_samples, out_fault_code);
#endif
}

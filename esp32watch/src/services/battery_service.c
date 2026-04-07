#include "services/battery_service.h"
#include "app_config.h"
#include "app_tuning.h"
#include "board_features.h"
#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "model.h"
#include "services/diag_service.h"
#include "system_init.h"
#include "platform_api.h"

static bool g_battery_service_initialized;

#if APP_FEATURE_BATTERY
static uint32_t g_last_battery_sample_ms;
static uint16_t g_last_battery_mv;
static uint8_t g_last_battery_percent;

static bool battery_is_charging(void)
{
#if CHARGE_DET_ENABLED
    return !bsp_gpio_read(CHARGE_DET_GPIO_Port, CHARGE_DET_Pin);
#else
    return false;
#endif
}

static uint16_t battery_adc_to_mv(uint32_t adc)
{
    uint32_t numerator;
    uint32_t denominator;
    uint32_t mv;

    numerator = adc * 3300UL * BATTERY_DIVIDER_NUMERATOR;
    denominator = 4095UL * BATTERY_DIVIDER_DENOMINATOR;
    mv = (numerator + (denominator / 2UL)) / denominator;
    if (mv > 0xFFFFUL) {
        mv = 0xFFFFUL;
    }
    return (uint16_t)mv;
}

uint8_t battery_service_estimate_percent(uint16_t mv)
{
    static const struct {
        uint16_t mv;
        uint8_t percent;
    } table[] = {
        {3300U, 0U}, {3450U, 5U}, {3550U, 12U}, {3650U, 25U}, {3720U, 40U},
        {3780U, 55U}, {3850U, 70U}, {3920U, 82U}, {4000U, 92U}, {4100U, 98U}, {4200U, 100U}
    };
    if (mv <= table[0].mv) return table[0].percent;
    for (uint32_t i = 1U; i < (sizeof(table) / sizeof(table[0])); ++i) {
        if (mv <= table[i].mv) {
            uint16_t x0 = table[i - 1U].mv;
            uint16_t x1 = table[i].mv;
            uint8_t y0 = table[i - 1U].percent;
            uint8_t y1 = table[i].percent;
            return (uint8_t)(y0 + (((uint32_t)(mv - x0) * (uint32_t)(y1 - y0)) / (uint32_t)(x1 - x0)));
        }
    }
    return 100U;
}

static uint16_t battery_filter_mv(uint16_t mv)
{
    if (g_last_battery_mv == 0U) {
        return mv;
    }
    return (uint16_t)((g_last_battery_mv * 3U + mv) / 4U);
}

uint8_t battery_service_apply_percent_hysteresis(uint8_t previous_percent, uint8_t estimated_percent, bool charging)
{
    if (charging) {
        if (estimated_percent < previous_percent) {
            return previous_percent;
        }
        if ((uint8_t)(estimated_percent - previous_percent) <= 1U) {
            return previous_percent;
        }
        return estimated_percent;
    }

    if (estimated_percent > previous_percent) {
        return previous_percent;
    }
    if ((uint8_t)(previous_percent - estimated_percent) <= 1U) {
        return previous_percent;
    }
    return estimated_percent;
}

static void battery_publish(uint16_t mv)
{
    bool charging = battery_is_charging();
    uint16_t filtered_mv = battery_filter_mv(mv);
    uint8_t percent = battery_service_estimate_percent(filtered_mv);
    if (g_last_battery_mv != 0U) {
        percent = battery_service_apply_percent_hysteresis(g_last_battery_percent, percent, charging);
    }
    model_set_battery(filtered_mv, percent, charging, true);
    diag_service_note_battery_sample(filtered_mv, percent);
    g_last_battery_mv = filtered_mv;
    g_last_battery_percent = percent;
}
#endif

void battery_service_init(void)
{
    g_battery_service_initialized = true;
#if APP_FEATURE_BATTERY
    g_last_battery_sample_ms = platform_time_now_ms();
    g_last_battery_mv = 0U;
    g_last_battery_percent = 0U;
    model_set_battery(0U, 0U, false, false);
    if (!system_runtime_battery_adc_available()) {
        return;
    }
#else
    model_set_battery(0U, 0U, false, false);
#endif
}

void battery_service_sample_now(void)
{
#if APP_FEATURE_BATTERY
    uint32_t average = 0U;
    if (!system_runtime_battery_adc_available()) {
        model_set_battery(0U, 0U, false, false);
        return;
    }
    uint8_t valid_samples = 0U;
    uint16_t fault_code = 0U;

    if (bsp_adc_sample_trimmed_mean(&platform_adc_main, 8U, 10U, &average, &valid_samples, &fault_code)) {
        battery_publish(battery_adc_to_mv(average));
    } else {
        diag_service_note_battery_fault(fault_code != 0U ? fault_code : 0xFFFFU);
        model_set_battery(0U, 0U, false, false);
    }
    (void)valid_samples;
    g_last_battery_sample_ms = platform_time_now_ms();
#endif
}

void battery_service_tick(uint32_t now_ms)
{
#if APP_FEATURE_BATTERY
    if (!system_runtime_battery_adc_available()) {
        (void)now_ms;
        return;
    }
    if ((now_ms - g_last_battery_sample_ms) >= app_tuning_manifest_get()->battery_sample_ms) {
        battery_service_sample_now();
    }
#else
    (void)now_ms;
#endif
}


bool battery_service_is_initialized(void)
{
    return g_battery_service_initialized;
}



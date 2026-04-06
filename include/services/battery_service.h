#ifndef BATTERY_SERVICE_H
#define BATTERY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

uint8_t battery_service_estimate_percent(uint16_t mv);
uint8_t battery_service_apply_percent_hysteresis(uint8_t previous_percent, uint8_t estimated_percent, bool charging);
void battery_service_init(void);
void battery_service_tick(uint32_t now_ms);
void battery_service_sample_now(void);

bool battery_service_is_initialized(void);

#endif

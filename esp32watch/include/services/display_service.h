#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"
#include "services/device_config.h"

void display_service_init(void);
void display_service_apply_settings(const SettingsState *settings);
void display_service_set_sleeping(bool sleeping);
void display_service_begin_frame(void);
void display_service_end_frame(void);
void display_service_force_full_refresh(void);

bool display_service_is_initialized(void);

uint32_t display_service_last_applied_generation(void);
bool display_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg);

#endif

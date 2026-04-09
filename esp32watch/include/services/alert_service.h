#ifndef ALERT_SERVICE_H
#define ALERT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

void alert_service_init(void);
void alert_service_tick(uint32_t now_ms);

bool alert_service_is_initialized(void);
void alert_service_stop_audio(void);

#endif

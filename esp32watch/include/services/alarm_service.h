#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

void alarm_service_init(void);
void alarm_service_tick(uint32_t now_ms);

bool alarm_service_is_initialized(void);

#endif

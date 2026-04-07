#ifndef ACTIVITY_SERVICE_H
#define ACTIVITY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "sensor.h"

void activity_service_init(void);
void activity_service_ingest_snapshot(const SensorSnapshot *snap, uint32_t now_ms);

bool activity_service_is_initialized(void);

#endif

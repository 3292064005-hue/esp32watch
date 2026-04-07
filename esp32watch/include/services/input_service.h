#ifndef INPUT_SERVICE_H
#define INPUT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "key.h"

void input_service_init(void);
void input_service_tick(void);
bool input_service_pop_event(KeyEvent *event);
bool input_service_is_down(KeyId id);
uint16_t input_service_get_overflow_count(void);

bool input_service_is_initialized(void);

#endif

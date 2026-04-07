#include "services/input_service.h"
#include "key.h"

static bool g_input_initialized;


void input_service_init(void)
{
    key_init();
    g_input_initialized = true;
}

void input_service_tick(void)
{
    key_scan_10ms();
}

bool input_service_pop_event(KeyEvent *event)
{
    return key_pop_event(event);
}

bool input_service_is_down(KeyId id)
{
    return key_is_down(id);
}

uint16_t input_service_get_overflow_count(void)
{
    return key_get_overflow_count();
}


bool input_service_is_initialized(void)
{
    return g_input_initialized;
}

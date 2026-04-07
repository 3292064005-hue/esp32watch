#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool web_server_init(void);
void web_server_poll(uint32_t now_ms);
bool web_server_is_ready(void);

#ifdef __cplusplus
}
#endif

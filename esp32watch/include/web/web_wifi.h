#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool web_wifi_init(void);
void web_wifi_tick(uint32_t now_ms);
bool web_wifi_connected(void);
const char *web_wifi_ip_string(void);
int32_t web_wifi_rssi(void);

#ifdef __cplusplus
}
#endif

#ifndef NETWORK_SYNC_SERVICE_H
#define NETWORK_SYNC_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool wifi_connected;
    bool time_synced;
    uint32_t last_time_sync_ms;
    bool weather_valid;
    int16_t temperature_tenths_c;
    uint8_t weather_code;
    char weather_text[20];
    char location_name[24];
    uint32_t last_weather_sync_ms;
} NetworkSyncSnapshot;

void network_sync_service_init(void);
void network_sync_service_tick(uint32_t now_ms);
void network_sync_service_request_refresh(void);
bool network_sync_service_get_snapshot(NetworkSyncSnapshot *out);
const char *network_sync_service_weather_text(uint8_t weather_code);

#ifdef __cplusplus
}
#endif

#endif

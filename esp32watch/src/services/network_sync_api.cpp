#include <math.h>
#include <string.h>

extern "C" {
#include "services/network_sync_service.h"
}

#include "services/network_sync_internal.hpp"

extern "C" void network_sync_service_request_refresh(void)
{
    g_network_sync.force_refresh = true;
}

extern "C" bool network_sync_service_get_snapshot(NetworkSyncSnapshot *out)
{
    if (out == nullptr) {
        return false;
    }
    *out = g_network_sync.snapshot;
    return true;
}

extern "C" uint32_t network_sync_service_last_applied_generation(void)
{
    return g_network_sync.last_applied_generation;
}

extern "C" bool network_sync_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    if (!g_network_sync.initialized || generation == 0U || generation != g_network_sync.last_applied_generation) {
        return false;
    }
    if (cfg == nullptr) {
        return true;
    }
    if (strcmp(g_network_sync.active_tz_posix, cfg->timezone_posix) != 0) {
        return false;
    }
    if (strcmp(g_network_sync.weather_tz_id, cfg->timezone_id) != 0 ||
        strcmp(g_network_sync.weather_location_name, cfg->location_name) != 0) {
        return false;
    }
    if (fabsf(g_network_sync.weather_latitude - cfg->latitude) > 0.000001f ||
        fabsf(g_network_sync.weather_longitude - cfg->longitude) > 0.000001f) {
        return false;
    }
    return true;
}

#if defined(HOST_RUNTIME_TEST)
extern "C" void network_sync_service_test_seed_weather_state(uint32_t generation,
                                                              const char *location,
                                                              const char *status)
{
    g_network_sync.weather_generation = generation == 0U ? 1U : generation;
    g_network_sync.last_applied_generation = generation == 0U ? 1U : generation;
    network_sync_copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                location != nullptr ? location : "");
    network_sync_copy_string(g_network_sync.snapshot.weather_status,
                sizeof(g_network_sync.snapshot.weather_status),
                status != nullptr ? status : "PENDING");
}

extern "C" void network_sync_service_test_inject_weather_result(uint32_t generation,
                                                                 bool success,
                                                                 const char *location,
                                                                 const char *status,
                                                                 int16_t temperature_tenths_c,
                                                                 uint8_t weather_code,
                                                                 uint32_t completed_at_ms)
{
    WeatherFetchResult result = {};
    result.completed = true;
    result.success = success;
    result.last_attempt_ok = success;
    result.temperature_tenths_c = temperature_tenths_c;
    result.weather_code = weather_code;
    result.completed_at_ms = completed_at_ms;
    result.http_status = success ? 200 : -1;
    result.ca_loaded = true;
    result.tls_verified = true;
    result.generation = generation;
    network_sync_copy_string(result.location_name, sizeof(result.location_name), location != nullptr ? location : "");
    network_sync_copy_string(result.weather_text, sizeof(result.weather_text), network_sync_service_weather_text(weather_code));
    network_sync_copy_string(result.tls_mode, sizeof(result.tls_mode), "STRICT");
    network_sync_copy_string(result.status, sizeof(result.status), status != nullptr ? status : (success ? "SYNCED" : "ERROR"));
    if (weather_lock_acquire()) {
        g_network_sync.weather_result = result;
        g_network_sync.weather_result_pending = true;
        weather_lock_release();
    }
}

extern "C" void network_sync_service_test_apply_pending_weather_result(void)
{
    network_sync_apply_weather_result();
}
#endif

extern "C" const char *network_sync_service_weather_text(uint8_t weather_code)
{
    switch (weather_code) {
        case 0: return "CLEAR";
        case 1: return "MAINLY CLR";
        case 2: return "PART CLOUD";
        case 3: return "OVERCAST";
        case 45:
        case 48: return "FOG";
        case 51:
        case 53:
        case 55: return "DRIZZLE";
        case 56:
        case 57: return "FRZ DRIZ";
        case 61:
        case 63:
        case 65: return "RAIN";
        case 66:
        case 67: return "FRZ RAIN";
        case 71:
        case 73:
        case 75:
        case 77: return "SNOW";
        case 80:
        case 81:
        case 82: return "SHOWERS";
        case 85:
        case 86: return "SNOW SHWR";
        case 95: return "TSTORM";
        case 96:
        case 99: return "TSTRM HAIL";
        default: return "UNKNOWN";
    }
}

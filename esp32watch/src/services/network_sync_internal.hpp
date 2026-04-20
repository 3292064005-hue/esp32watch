#ifndef NETWORK_SYNC_INTERNAL_HPP
#define NETWORK_SYNC_INTERNAL_HPP

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

extern "C" {
#include "services/network_sync_service.h"
#include "services/device_config.h"
#include "services/runtime_event_service.h"
}

inline constexpr const char *kNtpServer1 = "ntp.aliyun.com";
inline constexpr const char *kNtpServer2 = "ntp.ntsc.ac.cn";
inline constexpr const char *kNtpServer3 = "pool.ntp.org";
inline constexpr const char *kWeatherBaseUrl = "https://api.open-meteo.com/v1/forecast";
inline constexpr uint32_t kWeatherWorkerPollDelayMs = 25U;
inline constexpr uint32_t kWeatherWorkerStackDepth = 6144U;
inline constexpr UBaseType_t kWeatherWorkerPriority = 1U;

extern const char *const kWeatherCaBundleStart;
extern const char *const kWeatherCaBundleEnd;

struct WeatherFetchResult {
    bool completed;
    bool success;
    bool tls_verified;
    bool ca_loaded;
    bool last_attempt_ok;
    int32_t http_status;
    int16_t temperature_tenths_c;
    uint8_t weather_code;
    uint32_t completed_at_ms;
    char weather_text[20];
    char location_name[24];
    char tls_mode[16];
    char status[20];
    uint32_t generation;
};

struct NetworkSyncState {
    NetworkSyncSnapshot snapshot;
    bool initialized;
    bool ntp_started;
    bool force_refresh;
    uint32_t last_time_attempt_ms;
    uint32_t last_weather_attempt_ms;
    char active_tz_posix[DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN + 1U];
    char weather_tz_id[DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN + 1U];
    char weather_location_name[DEVICE_CONFIG_LOCATION_NAME_MAX_LEN + 1U];
    float weather_latitude;
    float weather_longitude;
    TaskHandle_t weather_worker;
    SemaphoreHandle_t weather_lock;
    volatile bool weather_worker_ready;
    volatile bool weather_job_pending;
    volatile bool weather_job_inflight;
    volatile bool weather_result_pending;
    uint32_t weather_generation;
    uint32_t weather_job_generation;
    uint32_t last_applied_generation;
    uint32_t pending_apply_generation;
    DeviceConfigSnapshot weather_job_cfg;
    WeatherFetchResult weather_result;
};

extern NetworkSyncState g_network_sync;

bool weather_lock_acquire(void);
void weather_lock_release(void);
void network_sync_copy_string(char *dst, size_t dst_size, const char *src);
void set_weather_status(const char *status);
void update_tls_snapshot(bool strict_mode, bool ca_loaded);
void set_result_tls_snapshot(WeatherFetchResult *result, bool strict_mode, bool ca_loaded);
const char *weather_ca_bundle_ptr(size_t *out_len);
void clear_weather_snapshot(const DeviceConfigSnapshot &cfg);
bool weather_profile_changed(const DeviceConfigSnapshot &cfg);
void cache_weather_profile(const DeviceConfigSnapshot &cfg);
void note_applied_generation(void);
void network_sync_advance_weather_generation(void);
void network_sync_cancel_weather_work(void);
bool network_sync_should_accept_weather_result(uint32_t result_generation);
void network_sync_reconcile_config(const DeviceConfigSnapshot &cfg);
bool network_sync_refresh_config(DeviceConfigSnapshot *cfg);
void network_sync_configure_ntp_if_needed(const DeviceConfigSnapshot &cfg);
bool network_sync_try_time_sync(uint32_t now_ms, const DeviceConfigSnapshot &cfg);
bool network_sync_prepare_weather_client(WiFiClientSecure *client, WeatherFetchResult *result);
bool network_sync_fetch_weather_blocking(uint32_t now_ms,
                                         const DeviceConfigSnapshot *cfg,
                                         uint32_t generation,
                                         WeatherFetchResult *result);
void network_sync_weather_worker(void *arg);
bool network_sync_start_weather_worker(void);
void network_sync_apply_weather_result(void);
void network_sync_queue_weather_job(const DeviceConfigSnapshot &cfg);
bool apply_config_changed(void);
bool handle_runtime_event(RuntimeServiceEvent event, void *ctx);
void network_sync_service_cleanup_init_failure(void);

#endif

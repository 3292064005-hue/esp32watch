#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

extern "C" {
#include "services/storage_facade.h"
#include "services/network_sync_service.h"
#include "services/network_sync_build_config.h"
#include "services/time_service.h"
#include "services/runtime_event_service.h"
#include "web/web_wifi.h"
}

namespace {
constexpr const char *kNtpServer1 = "ntp.aliyun.com";
constexpr const char *kNtpServer2 = "ntp.ntsc.ac.cn";
constexpr const char *kNtpServer3 = "pool.ntp.org";
constexpr const char *kWeatherBaseUrl = "https://api.open-meteo.com/v1/forecast";
constexpr uint32_t kWeatherWorkerPollDelayMs = 25U;
constexpr uint32_t kWeatherWorkerStackDepth = 6144U;
constexpr UBaseType_t kWeatherWorkerPriority = 1U;

extern const char kWeatherCaBundleStart[] asm("_binary_data_certs_weather_ca_bundle_pem_start");
extern const char kWeatherCaBundleEnd[] asm("_binary_data_certs_weather_ca_bundle_pem_end");

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

NetworkSyncState g_network_sync = {};

static bool weather_lock_acquire(void)
{
    if (g_network_sync.weather_lock == nullptr) {
        return false;
    }
    return xSemaphoreTake(g_network_sync.weather_lock, portMAX_DELAY) == pdTRUE;
}

static void weather_lock_release(void)
{
    if (g_network_sync.weather_lock != nullptr) {
        (void)xSemaphoreGive(g_network_sync.weather_lock);
    }
}

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst == nullptr || dst_size == 0U) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static void set_weather_status(const char *status)
{
    copy_string(g_network_sync.snapshot.weather_status,
                sizeof(g_network_sync.snapshot.weather_status),
                status);
}

static void update_tls_snapshot(bool strict_mode, bool ca_loaded)
{
    g_network_sync.snapshot.weather_ca_loaded = ca_loaded;
    g_network_sync.snapshot.weather_tls_verified = strict_mode && ca_loaded;
    copy_string(g_network_sync.snapshot.weather_tls_mode,
                sizeof(g_network_sync.snapshot.weather_tls_mode),
                strict_mode ? "STRICT" : "INSECURE");
}

static void set_result_tls_snapshot(WeatherFetchResult *result, bool strict_mode, bool ca_loaded)
{
    if (result == nullptr) {
        return;
    }
    result->tls_verified = strict_mode && ca_loaded;
    result->ca_loaded = ca_loaded;
    copy_string(result->tls_mode, sizeof(result->tls_mode), strict_mode ? "STRICT" : "INSECURE");
}

static const char *weather_ca_bundle_ptr(size_t *out_len)
{
    size_t len = 0U;
    if (kWeatherCaBundleEnd > kWeatherCaBundleStart) {
        len = (size_t)(kWeatherCaBundleEnd - kWeatherCaBundleStart);
    }
    if (out_len != nullptr) {
        *out_len = len;
    }
    return len > 1U ? kWeatherCaBundleStart : nullptr;
}

static bool parse_weather_payload(const String &payload, float *out_temp_c, uint8_t *out_weather_code)
{
    StaticJsonDocument<1024> doc;
    JsonVariant current;
    float temp_c;
    int weather_code;

    if (out_temp_c == nullptr || out_weather_code == nullptr) {
        return false;
    }

    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[NET] Weather JSON parse error: %s\n", err.c_str());
        return false;
    }

    current = doc["current"];
    if (current.isNull() || !current.is<JsonObject>()) {
        Serial.println("[NET] Weather payload missing current object");
        return false;
    }

    if (!current["temperature_2m"].is<float>() && !current["temperature_2m"].is<double>() && !current["temperature_2m"].is<int>()) {
        Serial.println("[NET] Weather payload missing temperature_2m");
        return false;
    }
    if (!current["weather_code"].is<int>()) {
        Serial.println("[NET] Weather payload missing weather_code");
        return false;
    }

    temp_c = current["temperature_2m"].as<float>();
    weather_code = current["weather_code"].as<int>();
    if (!isfinite(temp_c) || weather_code < 0 || weather_code > 255) {
        Serial.println("[NET] Weather payload contained invalid values");
        return false;
    }

    *out_temp_c = temp_c;
    *out_weather_code = (uint8_t)weather_code;
    return true;
}

static void append_url_encoded(String &dst, const char *src)
{
    static const char *kHex = "0123456789ABCDEF";

    if (src == nullptr) {
        return;
    }
    while (*src != '\0') {
        uint8_t ch = (uint8_t)*src++;
        bool safe = (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '-' || ch == '_' || ch == '.' || ch == '~';
        if (safe) {
            dst += (char)ch;
            continue;
        }
        dst += '%';
        dst += kHex[(ch >> 4) & 0x0F];
        dst += kHex[ch & 0x0F];
    }
}

static void clear_weather_snapshot(const DeviceConfigSnapshot &cfg)
{
    g_network_sync.snapshot.weather_valid = false;
    g_network_sync.snapshot.temperature_tenths_c = 0;
    g_network_sync.snapshot.weather_code = 0U;
    g_network_sync.snapshot.last_weather_sync_ms = 0U;
    g_network_sync.snapshot.weather_last_attempt_ok = false;
    g_network_sync.snapshot.last_weather_http_status = 0U;
    copy_string(g_network_sync.snapshot.weather_text,
                sizeof(g_network_sync.snapshot.weather_text),
                cfg.weather_configured ? "SYNC NEEDED" : "UNSET");
    copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                cfg.location_name);
    set_weather_status(cfg.weather_configured ? "PENDING" : "UNSET");
}

static bool weather_profile_changed(const DeviceConfigSnapshot &cfg)
{
    return strcmp(g_network_sync.weather_tz_id, cfg.timezone_id) != 0 ||
           strcmp(g_network_sync.weather_location_name, cfg.location_name) != 0 ||
           fabsf(g_network_sync.weather_latitude - cfg.latitude) > 0.000001f ||
           fabsf(g_network_sync.weather_longitude - cfg.longitude) > 0.000001f;
}

static void cache_weather_profile(const DeviceConfigSnapshot &cfg)
{
    copy_string(g_network_sync.weather_tz_id, sizeof(g_network_sync.weather_tz_id), cfg.timezone_id);
    copy_string(g_network_sync.weather_location_name, sizeof(g_network_sync.weather_location_name), cfg.location_name);
    g_network_sync.weather_latitude = cfg.latitude;
    g_network_sync.weather_longitude = cfg.longitude;
    copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                cfg.location_name);
}

static void note_applied_generation(void)
{
    g_network_sync.last_applied_generation = device_config_generation();
}

static void network_sync_advance_weather_generation(void)
{
    if (g_network_sync.weather_generation == UINT32_MAX) {
        g_network_sync.weather_generation = 1U;
        return;
    }
    ++g_network_sync.weather_generation;
    if (g_network_sync.weather_generation == 0U) {
        g_network_sync.weather_generation = 1U;
    }
}

static void network_sync_cancel_weather_work(void)
{
    network_sync_advance_weather_generation();
    if (weather_lock_acquire()) {
        g_network_sync.weather_job_pending = false;
        g_network_sync.weather_job_inflight = false;
        g_network_sync.weather_result_pending = false;
        g_network_sync.weather_job_generation = 0U;
        memset(&g_network_sync.weather_result, 0, sizeof(g_network_sync.weather_result));
        weather_lock_release();
    }
}

static bool network_sync_should_accept_weather_result(uint32_t result_generation)
{
    return result_generation != 0U && result_generation == g_network_sync.weather_generation;
}

static void network_sync_reconcile_config(const DeviceConfigSnapshot &cfg)
{
    if (!g_network_sync.ntp_started || strcmp(g_network_sync.active_tz_posix, cfg.timezone_posix) != 0) {
        g_network_sync.snapshot.time_synced = false;
    }

    if (!cfg.weather_configured) {
        clear_weather_snapshot(cfg);
        cache_weather_profile(cfg);
        network_sync_cancel_weather_work();
        return;
    }

    if (weather_profile_changed(cfg)) {
        clear_weather_snapshot(cfg);
        cache_weather_profile(cfg);
        network_sync_cancel_weather_work();
    }
}

static bool network_sync_refresh_config(DeviceConfigSnapshot *cfg)
{
    if (cfg == nullptr) {
        return false;
    }

    memset(cfg, 0, sizeof(*cfg));
    if (!storage_facade_get_device_config(cfg)) {
        g_network_sync.snapshot.time_synced = false;
        g_network_sync.snapshot.weather_valid = false;
        g_network_sync.snapshot.weather_last_attempt_ok = false;
        g_network_sync.snapshot.last_weather_http_status = 0;
        copy_string(g_network_sync.snapshot.location_name,
                    sizeof(g_network_sync.snapshot.location_name),
                    "");
        copy_string(g_network_sync.weather_tz_id, sizeof(g_network_sync.weather_tz_id), "");
        copy_string(g_network_sync.weather_location_name, sizeof(g_network_sync.weather_location_name), "");
        g_network_sync.weather_latitude = 0.0f;
        g_network_sync.weather_longitude = 0.0f;
        set_weather_status("CFG_ERR");
        return false;
    }

    network_sync_reconcile_config(*cfg);
    g_network_sync.pending_apply_generation = device_config_generation();
    return true;
}

static void network_sync_configure_ntp_if_needed(const DeviceConfigSnapshot &cfg)
{
    if (!g_network_sync.ntp_started || strcmp(g_network_sync.active_tz_posix, cfg.timezone_posix) != 0) {
        configTzTime(cfg.timezone_posix, kNtpServer1, kNtpServer2, kNtpServer3);
        copy_string(g_network_sync.active_tz_posix, sizeof(g_network_sync.active_tz_posix), cfg.timezone_posix);
        g_network_sync.ntp_started = true;
        Serial.printf("[NET] NTP configured tz_posix=%s\n", g_network_sync.active_tz_posix);
    }
}

static bool network_sync_try_time_sync(uint32_t now_ms, const DeviceConfigSnapshot &cfg)
{
    struct tm local_tm;
    DateTime dt;

    network_sync_configure_ntp_if_needed(cfg);
    if (!getLocalTime(&local_tm, 100)) {
        return false;
    }

    dt.year = (uint16_t)(local_tm.tm_year + 1900);
    dt.month = (uint8_t)(local_tm.tm_mon + 1);
    dt.day = (uint8_t)local_tm.tm_mday;
    dt.weekday = (uint8_t)local_tm.tm_wday;
    dt.hour = (uint8_t)local_tm.tm_hour;
    dt.minute = (uint8_t)local_tm.tm_min;
    dt.second = (uint8_t)local_tm.tm_sec;

    if (!time_service_try_set_datetime(&dt, TIME_SOURCE_NETWORK_SYNC)) {
        return false;
    }

    g_network_sync.snapshot.time_synced = true;
    g_network_sync.snapshot.last_time_sync_ms = now_ms;
    Serial.printf("[NET] Time synced: %04u-%02u-%02u %02u:%02u:%02u tz=%s\n",
                  (unsigned)dt.year,
                  (unsigned)dt.month,
                  (unsigned)dt.day,
                  (unsigned)dt.hour,
                  (unsigned)dt.minute,
                  (unsigned)dt.second,
                  cfg.timezone_posix);
    return true;
}

static String network_sync_build_weather_url(const DeviceConfigSnapshot &cfg)
{
    String url;
    char lat_buf[24];
    char lon_buf[24];

    snprintf(lat_buf, sizeof(lat_buf), "%.6f", cfg.latitude);
    snprintf(lon_buf, sizeof(lon_buf), "%.6f", cfg.longitude);

    url.reserve(192);
    url += kWeatherBaseUrl;
    url += "?latitude=";
    url += lat_buf;
    url += "&longitude=";
    url += lon_buf;
    url += "&current=temperature_2m,weather_code";
    url += "&timezone=";
    append_url_encoded(url, cfg.timezone_id);
    url += "&forecast_days=1";
    return url;
}

static bool network_sync_prepare_weather_client(WiFiClientSecure *client, WeatherFetchResult *result)
{
    size_t bundle_len = 0U;
    const char *bundle = nullptr;

    if (client == nullptr || result == nullptr) {
        return false;
    }

    bundle = weather_ca_bundle_ptr(&bundle_len);
    if (bundle != nullptr) {
        client->setCACert(bundle);
        set_result_tls_snapshot(result, true, true);
        return true;
    }

#if APP_NETWORK_SYNC_TLS_ALLOW_INSECURE
    client->setInsecure();
    set_result_tls_snapshot(result, false, false);
    copy_string(result->status, sizeof(result->status), "TLS DEV");
    Serial.println("[NET] Weather TLS running in insecure development mode");
    return true;
#else
    set_result_tls_snapshot(result, true, false);
    copy_string(result->status, sizeof(result->status), "CA_MISSING");
    Serial.println("[NET] Weather TLS strict mode blocked because the embedded CA bundle is unavailable");
    return false;
#endif
}

static bool network_sync_fetch_weather_blocking(uint32_t now_ms,
                                                const DeviceConfigSnapshot &cfg,
                                                WeatherFetchResult *out)
{
    WiFiClientSecure client;
    HTTPClient http;
    String payload;
    String url;
    float temp_c = 0.0f;
    uint8_t weather_code = 0U;
    int status_code;

    if (out == nullptr) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->http_status = 0;
    copy_string(out->location_name, sizeof(out->location_name), cfg.location_name);

    if (!cfg.weather_configured) {
        copy_string(out->weather_text, sizeof(out->weather_text), "UNSET");
        copy_string(out->status, sizeof(out->status), "UNSET");
        out->completed = true;
        return false;
    }

    if (!network_sync_prepare_weather_client(&client, out)) {
        out->last_attempt_ok = false;
        out->http_status = -1;
        out->completed = true;
        return false;
    }

    url = network_sync_build_weather_url(cfg);
    http.setConnectTimeout(APP_NETWORK_SYNC_WEATHER_CONNECT_TIMEOUT_MS);
    http.setTimeout(APP_NETWORK_SYNC_WEATHER_REQUEST_TIMEOUT_MS);

    if (!http.begin(client, url)) {
        copy_string(out->status, sizeof(out->status), "BEGIN_ERR");
        out->last_attempt_ok = false;
        out->http_status = -1;
        out->completed = true;
        return false;
    }

    status_code = http.GET();
    out->http_status = status_code;
    if (status_code != HTTP_CODE_OK) {
        Serial.printf("[NET] Weather HTTP error: %d\n", status_code);
        copy_string(out->status, sizeof(out->status), "HTTP_ERR");
        out->last_attempt_ok = false;
        http.end();
        out->completed = true;
        return false;
    }

    payload = http.getString();
    http.end();

    if (!parse_weather_payload(payload, &temp_c, &weather_code)) {
        Serial.println("[NET] Weather payload parse failed");
        copy_string(out->status, sizeof(out->status), "PARSE_ERR");
        out->last_attempt_ok = false;
        out->completed = true;
        return false;
    }

    out->success = true;
    out->last_attempt_ok = true;
    out->temperature_tenths_c = (int16_t)lroundf(temp_c * 10.0f);
    out->weather_code = weather_code;
    out->completed_at_ms = now_ms;
    copy_string(out->weather_text, sizeof(out->weather_text), network_sync_service_weather_text(weather_code));
    copy_string(out->status, sizeof(out->status), "SYNCED");
    out->completed = true;

    Serial.printf("[NET] Weather synced: %s %d.%dC location=%s tls=%s\n",
                  out->weather_text,
                  (int)(out->temperature_tenths_c / 10),
                  abs((int)(out->temperature_tenths_c % 10)),
                  out->location_name,
                  out->tls_mode);
    return true;
}

/**
 * @brief Background weather worker entry point.
 *
 * Runs blocking HTTPS weather requests outside the cooperative main loop. The worker only
 * reads queued configuration snapshots and publishes immutable results back to the main loop,
 * keeping all model-visible state mutation in the main-thread tick path.
 *
 * @param[in] arg Unused.
 * @return Never returns.
 * @throws None.
 * @boundary_behavior Sleeps when no work is queued; retains the last queued job until the main loop consumes the result.
 */
static void network_sync_weather_worker(void *arg)
{
    (void)arg;
    g_network_sync.weather_worker_ready = true;

    for (;;) {
        DeviceConfigSnapshot cfg = {};
        WeatherFetchResult result = {};
        bool has_job = false;
        uint32_t job_generation = 0U;

        if (weather_lock_acquire()) {
            if (g_network_sync.weather_job_pending) {
                cfg = g_network_sync.weather_job_cfg;
                job_generation = g_network_sync.weather_job_generation;
                g_network_sync.weather_job_pending = false;
                g_network_sync.weather_job_inflight = true;
                has_job = true;
            }
            weather_lock_release();
        }

        if (!has_job) {
            vTaskDelay((TickType_t)(kWeatherWorkerPollDelayMs / portTICK_PERIOD_MS));
            continue;
        }

        (void)network_sync_fetch_weather_blocking(millis(), cfg, &result);
        result.generation = job_generation;
        if (weather_lock_acquire()) {
            g_network_sync.weather_result = result;
            g_network_sync.weather_result_pending = true;
            g_network_sync.weather_job_inflight = false;
            weather_lock_release();
        }
        vTaskDelay(1);
    }
}

static bool network_sync_start_weather_worker(void)
{
    if (g_network_sync.weather_worker != nullptr) {
        return true;
    }

    if (xTaskCreatePinnedToCore(network_sync_weather_worker,
                                "net_weather",
                                kWeatherWorkerStackDepth,
                                nullptr,
                                kWeatherWorkerPriority,
                                &g_network_sync.weather_worker,
                                1) != pdPASS) {
        Serial.println("[NET] Failed to start weather worker");
        g_network_sync.weather_worker = nullptr;
        return false;
    }
    return true;
}

static void network_sync_apply_weather_result(void)
{
    WeatherFetchResult result = {};

    if (!weather_lock_acquire()) {
        return;
    }
    if (!g_network_sync.weather_result_pending) {
        weather_lock_release();
        return;
    }

    result = g_network_sync.weather_result;
    g_network_sync.weather_result_pending = false;
    weather_lock_release();

    if (!network_sync_should_accept_weather_result(result.generation)) {
        Serial.printf("[NET] Discarding stale weather result generation=%lu active=%lu\n",
                      (unsigned long)result.generation,
                      (unsigned long)g_network_sync.weather_generation);
        return;
    }

    update_tls_snapshot(result.tls_verified, result.ca_loaded);
    g_network_sync.snapshot.weather_last_attempt_ok = result.last_attempt_ok;
    g_network_sync.snapshot.last_weather_http_status = result.http_status;

    if (!result.success) {
        if (result.status[0] != '\0') {
            set_weather_status(result.status);
        }
        return;
    }

    g_network_sync.snapshot.weather_valid = true;
    g_network_sync.snapshot.temperature_tenths_c = result.temperature_tenths_c;
    g_network_sync.snapshot.weather_code = result.weather_code;
    copy_string(g_network_sync.snapshot.weather_text,
                sizeof(g_network_sync.snapshot.weather_text),
                result.weather_text);
    copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                result.location_name);
    g_network_sync.snapshot.last_weather_sync_ms = result.completed_at_ms;
    set_weather_status(result.status[0] != '\0' ? result.status : "SYNCED");
}

static void network_sync_queue_weather_job(const DeviceConfigSnapshot &cfg)
{
    if (weather_lock_acquire()) {
        g_network_sync.weather_job_cfg = cfg;
        g_network_sync.weather_job_generation = g_network_sync.weather_generation;
        g_network_sync.weather_job_pending = true;
        g_network_sync.weather_result_pending = false;
        weather_lock_release();
    }
    set_weather_status("QUEUED");
}

static bool apply_config_changed(void)
{
    DeviceConfigSnapshot cfg;

    if (!g_network_sync.initialized) {
        return false;
    }
    if (!network_sync_refresh_config(&cfg)) {
        return false;
    }
    network_sync_configure_ntp_if_needed(cfg);
    cache_weather_profile(cfg);
    note_applied_generation();
    g_network_sync.pending_apply_generation = g_network_sync.last_applied_generation;
    g_network_sync.force_refresh = true;
    return true;
}

static bool handle_runtime_event(RuntimeServiceEvent event, void *ctx)
{
    (void)ctx;
    if (event != RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED) {
        return true;
    }
    return apply_config_changed();
}

static void network_sync_service_cleanup_init_failure(void)
{
    if (g_network_sync.weather_worker != nullptr) {
        vTaskDelete(g_network_sync.weather_worker);
        g_network_sync.weather_worker = nullptr;
        g_network_sync.weather_worker_ready = false;
    }
    if (g_network_sync.weather_lock != nullptr) {
        vSemaphoreDelete(g_network_sync.weather_lock);
        g_network_sync.weather_lock = nullptr;
    }
    (void)runtime_event_service_unregister(handle_runtime_event, nullptr);
}

} // namespace

extern "C" bool network_sync_service_init(void)
{
    DeviceConfigSnapshot cfg;

    memset(&g_network_sync, 0, sizeof(g_network_sync));
    g_network_sync.weather_generation = 1U;
    RuntimeEventSubscription subscription = {
        .handler = handle_runtime_event,
        .ctx = nullptr,
        .name = "network_sync",
        .priority = 10,
        .critical = false,
    };
    if (!runtime_event_service_register_ex(&subscription)) {
        return false;
    }
    g_network_sync.weather_lock = xSemaphoreCreateMutex();
    if (g_network_sync.weather_lock == nullptr) {
        (void)runtime_event_service_unregister(handle_runtime_event, nullptr);
        return false;
    }
    if (!network_sync_start_weather_worker()) {
        network_sync_service_cleanup_init_failure();
        return false;
    }
    g_network_sync.initialized = true;
    if (!network_sync_refresh_config(&cfg)) {
        g_network_sync.initialized = false;
        network_sync_service_cleanup_init_failure();
        return false;
    }
    network_sync_configure_ntp_if_needed(cfg);
    cache_weather_profile(cfg);
    note_applied_generation();
    g_network_sync.pending_apply_generation = g_network_sync.last_applied_generation;
    set_weather_status(cfg.weather_configured ? "PENDING" : "UNSET");
    update_tls_snapshot(true, weather_ca_bundle_ptr(nullptr) != nullptr);
    return true;
}

/**
 * @brief Advance the network synchronization service without blocking the cooperative runtime loop.
 *
 * Performs bounded work only: opportunistic NTP refresh, weather-worker result consumption,
 * and at most one weather job dispatch. Blocking HTTPS weather I/O is executed exclusively by
 * the dedicated worker task created during service initialization.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return void
 * @throws None.
 * @boundary_behavior Returns early when configuration is unavailable or Wi-Fi is offline, while retaining the last successful weather snapshot for diagnostics.
 */
extern "C" void network_sync_service_tick(uint32_t now_ms)
{
    DeviceConfigSnapshot cfg;
    bool force_refresh;
    bool want_time_sync;
    bool want_weather_sync;

    if (!network_sync_refresh_config(&cfg)) {
        return;
    }
    g_network_sync.snapshot.wifi_connected = web_wifi_connected();
    network_sync_apply_weather_result();
    if (!g_network_sync.snapshot.wifi_connected) {
        return;
    }

    force_refresh = g_network_sync.force_refresh;
    want_time_sync = force_refresh ||
                     !g_network_sync.snapshot.time_synced ||
                     (now_ms - g_network_sync.snapshot.last_time_sync_ms) >= APP_NETWORK_SYNC_TIME_SYNC_PERIOD_MS;
    if (want_time_sync && (force_refresh || (now_ms - g_network_sync.last_time_attempt_ms) >= APP_NETWORK_SYNC_TIME_SYNC_RETRY_MS)) {
        g_network_sync.last_time_attempt_ms = now_ms;
        (void)network_sync_try_time_sync(now_ms, cfg);
    }

    want_weather_sync = cfg.weather_configured &&
                        (force_refresh ||
                         !g_network_sync.snapshot.weather_valid ||
                         (now_ms - g_network_sync.snapshot.last_weather_sync_ms) >= APP_NETWORK_SYNC_WEATHER_PERIOD_MS);
    bool weather_job_inflight = false;
    bool weather_job_pending = false;
    if (weather_lock_acquire()) {
        weather_job_inflight = g_network_sync.weather_job_inflight;
        weather_job_pending = g_network_sync.weather_job_pending;
        weather_lock_release();
    }
    if (weather_job_inflight) {
        set_weather_status("SYNCING");
    } else if (want_weather_sync && !weather_job_pending &&
               (force_refresh || (now_ms - g_network_sync.last_weather_attempt_ms) >= APP_NETWORK_SYNC_WEATHER_RETRY_MS)) {
        g_network_sync.last_weather_attempt_ms = now_ms;
        network_sync_queue_weather_job(cfg);
    }

    note_applied_generation();
    g_network_sync.force_refresh = false;
}

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
    copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                location != nullptr ? location : "");
    copy_string(g_network_sync.snapshot.weather_status,
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
    copy_string(result.location_name, sizeof(result.location_name), location != nullptr ? location : "");
    copy_string(result.weather_text, sizeof(result.weather_text), network_sync_service_weather_text(weather_code));
    copy_string(result.tls_mode, sizeof(result.tls_mode), "STRICT");
    copy_string(result.status, sizeof(result.status), status != nullptr ? status : (success ? "SYNCED" : "ERROR"));
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

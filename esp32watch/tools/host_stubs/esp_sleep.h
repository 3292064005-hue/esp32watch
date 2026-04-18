#pragma once
#include <stdint.h>

typedef int esp_err_t;

enum {
  ESP_OK = 0,
  ESP_ERR_INVALID_ARG = 0x102,
};

typedef enum {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0,
  ESP_SLEEP_WAKEUP_TIMER = 4,
} esp_sleep_wakeup_cause_t;

inline uint64_t host_stub_last_sleep_timer_us = 0;
inline bool host_stub_light_sleep_called = false;
inline esp_sleep_wakeup_cause_t host_stub_last_wakeup_cause = ESP_SLEEP_WAKEUP_TIMER;

inline void host_stub_reset_sleep() {
  host_stub_last_sleep_timer_us = 0;
  host_stub_light_sleep_called = false;
  host_stub_last_wakeup_cause = ESP_SLEEP_WAKEUP_TIMER;
}

inline esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
  host_stub_last_sleep_timer_us = time_in_us;
  return time_in_us == 0 ? ESP_ERR_INVALID_ARG : ESP_OK;
}
inline esp_err_t esp_light_sleep_start() {
  host_stub_light_sleep_called = true;
  return ESP_OK;
}
inline esp_err_t esp_sleep_disable_wakeup_source(int) { return ESP_OK; }
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return host_stub_last_wakeup_cause; }

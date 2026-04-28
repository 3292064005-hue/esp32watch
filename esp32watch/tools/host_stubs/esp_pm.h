#pragma once
#include <stdint.h>
#include "esp_sleep.h"

typedef void *esp_pm_lock_handle_t;

typedef enum {
  ESP_PM_CPU_FREQ_MAX = 0,
  ESP_PM_APB_FREQ_MAX = 1,
  ESP_PM_NO_LIGHT_SLEEP = 2,
} esp_pm_lock_type_t;

inline int host_stub_pm_lock_create_count = 0;
inline int host_stub_pm_lock_acquire_count = 0;
inline int host_stub_pm_lock_release_count = 0;
inline bool host_stub_pm_lock_create_fail = false;
inline bool host_stub_pm_lock_acquire_fail = false;
inline bool host_stub_pm_lock_release_fail = false;
inline uintptr_t host_stub_pm_lock_storage = 0x504D4C4BUL;

inline void host_stub_reset_pm_lock() {
  host_stub_pm_lock_create_count = 0;
  host_stub_pm_lock_acquire_count = 0;
  host_stub_pm_lock_release_count = 0;
  host_stub_pm_lock_create_fail = false;
  host_stub_pm_lock_acquire_fail = false;
  host_stub_pm_lock_release_fail = false;
  host_stub_pm_lock_storage = 0x504D4C4BUL;
}

inline esp_err_t esp_pm_lock_create(esp_pm_lock_type_t, int, const char *, esp_pm_lock_handle_t *out_handle) {
  ++host_stub_pm_lock_create_count;
  if (out_handle == nullptr || host_stub_pm_lock_create_fail) {
    return ESP_ERR_INVALID_ARG;
  }
  *out_handle = reinterpret_cast<esp_pm_lock_handle_t>(&host_stub_pm_lock_storage);
  return ESP_OK;
}

inline esp_err_t esp_pm_lock_acquire(esp_pm_lock_handle_t handle) {
  if (handle == nullptr || host_stub_pm_lock_acquire_fail) {
    return ESP_ERR_INVALID_ARG;
  }
  ++host_stub_pm_lock_acquire_count;
  return ESP_OK;
}

inline esp_err_t esp_pm_lock_release(esp_pm_lock_handle_t handle) {
  if (handle == nullptr || host_stub_pm_lock_release_fail) {
    return ESP_ERR_INVALID_ARG;
  }
  ++host_stub_pm_lock_release_count;
  return ESP_OK;
}

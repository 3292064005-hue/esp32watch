#pragma once
#include <stdint.h>
class ESPClass {
public:
  uint64_t getEfuseMac() const { return 0x1122334455667788ULL; }
  void restart() const {}
};
static ESPClass ESP;

inline uint32_t getCpuFrequencyMhz() { return 240U; }


typedef enum {
  ESP_RST_UNKNOWN = 0,
  ESP_RST_POWERON = 1,
  ESP_RST_EXT = 2,
  ESP_RST_SW = 3,
  ESP_RST_PANIC = 4,
  ESP_RST_INT_WDT = 5,
  ESP_RST_TASK_WDT = 6,
  ESP_RST_WDT = 7,
  ESP_RST_DEEPSLEEP = 8,
  ESP_RST_BROWNOUT = 9,
} esp_reset_reason_t;

inline esp_reset_reason_t host_stub_reset_reason = ESP_RST_POWERON;
inline esp_reset_reason_t esp_reset_reason() { return host_stub_reset_reason; }

inline uint32_t host_stub_esp_random_value = 0xA5A50001U;
inline uint32_t esp_random() { host_stub_esp_random_value = host_stub_esp_random_value * 1664525U + 1013904223U; return host_stub_esp_random_value; }

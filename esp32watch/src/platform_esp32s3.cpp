#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <string.h>
#include "esp32_port_config.h"
#if ESP32_POWER_USE_IDF_PM_LOCKS
#include <esp_pm.h>
#endif
#include "platform_api.h"
#include "app_config.h"
#include "board_manifest.h"

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

extern "C" {
PlatformI2cBus platform_i2c_main = {};
PlatformRtcDevice platform_rtc_main = {};
PlatformAdcDevice platform_adc_main = {};
PlatformUartPort platform_uart_main = {};
PlatformWatchdog platform_watchdog_main = {};
PlatformGpioPort g_platform_gpioa = {0};
PlatformGpioPort g_platform_gpiob = {1};
}

static constexpr uint32_t kRtcResetDomainMagic = 0xB4A0F11EU;
static constexpr uint32_t kBkpRegisterCount = 16U;

static RTC_NOINIT_ATTR uint32_t g_bkp_magic = 0U;
static RTC_NOINIT_ATTR uint32_t g_bkp_regs[kBkpRegisterCount] = {0};
static bool g_bkp_loaded = false;
static uint32_t g_adc_last_value = 2048U;
#if ESP32_POWER_USE_IDF_PM_LOCKS
static esp_pm_lock_handle_t g_platform_idle_pm_lock = nullptr;
#endif

static bool bkp_should_reset_for_reason(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:
#ifdef ESP_RST_BROWNOUT
        case ESP_RST_BROWNOUT:
#endif
        case ESP_RST_UNKNOWN:
            return true;
        default:
            return false;
    }
}

static int resolve_actual_gpio(PlatformGpioPort *port, uint16_t pin_mask)
{
    int manifest_gpio = board_manifest_resolve_native_gpio(port, pin_mask);
    if (manifest_gpio >= 0) {
        return manifest_gpio;
    }
    return -1;
}

static void bkp_load_once(void)
{
    if (g_bkp_loaded) {
        return;
    }

    if (g_bkp_magic != kRtcResetDomainMagic || bkp_should_reset_for_reason(esp_reset_reason())) {
        memset(g_bkp_regs, 0, sizeof(g_bkp_regs));
        g_bkp_magic = kRtcResetDomainMagic;
    }
    g_bkp_loaded = true;
}

static void bkp_store_index(uint32_t index)
{
    if (index >= kBkpRegisterCount) {
        return;
    }
    g_bkp_magic = kRtcResetDomainMagic;
}

void platform_init(void)
{
    bkp_load_once();
}

uint32_t platform_time_now_ms(void)
{
    return millis();
}

void platform_delay_ms(uint32_t delay_ms)
{
    delay(delay_ms);
}

void platform_gpio_init(PlatformGpioPort *port, const PlatformGpioConfig *config)
{
    if (config == nullptr) {
        return;
    }
    for (int bit = 0; bit < 16; ++bit) {
        uint16_t mask = (uint16_t)(1U << bit);
        if ((config->pin_mask & mask) == 0U) {
            continue;
        }
        int gpio = resolve_actual_gpio(port, mask);
        if (gpio < 0) {
            continue;
        }
        switch (config->mode) {
            case PLATFORM_GPIO_MODE_OUTPUT_PP:
                pinMode(gpio, OUTPUT);
                break;
            case PLATFORM_GPIO_MODE_OUTPUT_OD:
#ifdef OUTPUT_OPEN_DRAIN
                pinMode(gpio, OUTPUT_OPEN_DRAIN);
#else
                pinMode(gpio, OUTPUT);
#endif
                break;
            case PLATFORM_GPIO_MODE_INPUT:
            default:
                pinMode(gpio, (config->pull == PLATFORM_GPIO_PULL_UP) ? INPUT_PULLUP : INPUT);
                break;
        }
    }
}

PlatformPinState platform_gpio_read(PlatformGpioPort *port, uint16_t pin_mask)
{
    int gpio = resolve_actual_gpio(port, pin_mask);
    if (gpio < 0) {
        return PLATFORM_PIN_HIGH;
    }
    return digitalRead(gpio) ? PLATFORM_PIN_HIGH : PLATFORM_PIN_LOW;
}

void platform_gpio_write(PlatformGpioPort *port, uint16_t pin_mask, PlatformPinState state)
{
    for (int bit = 0; bit < 16; ++bit) {
        uint16_t mask = (uint16_t)(1U << bit);
        if ((pin_mask & mask) == 0U) {
            continue;
        }
        int gpio = resolve_actual_gpio(port, mask);
        if (gpio < 0) {
            continue;
        }
        digitalWrite(gpio, (state == PLATFORM_PIN_HIGH) ? HIGH : LOW);
    }
}

PlatformStatus platform_i2c_init(PlatformI2cBus *bus)
{
    const BoardManifest *manifest = board_manifest_get();
    (void)bus;
    if (manifest != nullptr && manifest->i2c_sda_gpio >= 0 && manifest->i2c_scl_gpio >= 0) {
        Wire.begin(manifest->i2c_sda_gpio, manifest->i2c_scl_gpio);
    } else {
        Wire.begin();
    }
    Wire.setClock((manifest != nullptr && manifest->i2c_clock_hz != 0U) ? manifest->i2c_clock_hz : 400000UL);
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_i2c_deinit(PlatformI2cBus *bus)
{
    (void)bus;
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_i2c_write(PlatformI2cBus *bus, uint16_t address, uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    (void)bus;
    (void)timeout_ms;
    Wire.beginTransmission((uint8_t)(address >> 1));
    size_t written = Wire.write(data, size);
    uint8_t err = Wire.endTransmission();
    return (written == size && err == 0U) ? PLATFORM_STATUS_OK : PLATFORM_STATUS_ERROR;
}

PlatformStatus platform_i2c_mem_read(PlatformI2cBus *bus, uint16_t address, uint16_t mem_address, uint16_t mem_address_size, uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    (void)bus;
    (void)mem_address_size;
    (void)timeout_ms;
    Wire.beginTransmission((uint8_t)(address >> 1));
    Wire.write((uint8_t)mem_address);
    if (Wire.endTransmission(false) != 0U) {
        return PLATFORM_STATUS_ERROR;
    }
    if (Wire.requestFrom((int)(address >> 1), (int)size) != size) {
        return PLATFORM_STATUS_ERROR;
    }
    for (uint16_t i = 0; i < size && Wire.available(); ++i) {
        data[i] = (uint8_t)Wire.read();
    }
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_rtc_init(PlatformRtcDevice *device)
{
    (void)device;
    bkp_load_once();
    return PLATFORM_STATUS_OK;
}

bool platform_rtc_wall_clock_supported(PlatformRtcDevice *device)
{
    (void)device;
    return false;
}

bool platform_rtc_wall_clock_persistent(PlatformRtcDevice *device)
{
    (void)device;
    return false;
}

PlatformStatus platform_rtc_get_epoch(PlatformRtcDevice *device, uint32_t *out_epoch)
{
    (void)device;
    if (out_epoch != nullptr) {
        *out_epoch = 0U;
    }
    return PLATFORM_STATUS_ERROR;
}

PlatformStatus platform_rtc_set_epoch(PlatformRtcDevice *device, uint32_t epoch)
{
    (void)device;
    (void)epoch;
    return PLATFORM_STATUS_ERROR;
}

void platform_rtc_backup_unlock(void)
{
}

uint32_t platform_rtc_backup_read(PlatformRtcDevice *device, uint32_t reg)
{
    (void)device;
    bkp_load_once();
    if (reg < kBkpRegisterCount) {
        return g_bkp_regs[reg];
    }
    return 0U;
}

void platform_rtc_backup_write(PlatformRtcDevice *device, uint32_t reg, uint32_t value)
{
    (void)device;
    bkp_load_once();
    if (reg < kBkpRegisterCount) {
        g_bkp_regs[reg] = value;
        bkp_store_index(reg);
    }
}

PlatformStatus platform_adc_init(PlatformAdcDevice *device)
{
    (void)device;
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_adc_config_channel(PlatformAdcDevice *device, void *channel_config)
{
    (void)device;
    (void)channel_config;
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_adc_calibrate(PlatformAdcDevice *device)
{
    (void)device;
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_adc_start(PlatformAdcDevice *device)
{
    (void)device;
    const BoardManifest *manifest = board_manifest_get();
    if (manifest != nullptr && manifest->battery_adc_gpio >= 0) {
        g_adc_last_value = (uint32_t)analogRead(manifest->battery_adc_gpio);
    }
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_adc_poll(PlatformAdcDevice *device, uint32_t timeout_ms)
{
    (void)device;
    (void)timeout_ms;
    return PLATFORM_STATUS_OK;
}

uint32_t platform_adc_read(PlatformAdcDevice *device)
{
    (void)device;
    return g_adc_last_value;
}

PlatformStatus platform_adc_stop(PlatformAdcDevice *device)
{
    (void)device;
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_watchdog_init(PlatformWatchdog *device)
{
    (void)device;
    return PLATFORM_STATUS_ERROR;
}

PlatformStatus platform_watchdog_kick(PlatformWatchdog *device)
{
    (void)device;
    return PLATFORM_STATUS_ERROR;
}


bool platform_pm_lock_supported(void)
{
#if ESP32_POWER_USE_IDF_PM_LOCKS
    return true;
#else
    return false;
#endif
}

bool platform_pm_lock_acquire(const char *owner)
{
    (void)owner;
#if ESP32_POWER_USE_IDF_PM_LOCKS
    if (g_platform_idle_pm_lock == nullptr) {
        if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "watch_idle", &g_platform_idle_pm_lock) != ESP_OK) {
            return false;
        }
    }
    return esp_pm_lock_acquire(g_platform_idle_pm_lock) == ESP_OK;
#else
    return true;
#endif
}

bool platform_pm_lock_release(const char *owner)
{
    (void)owner;
#if ESP32_POWER_USE_IDF_PM_LOCKS
    if (g_platform_idle_pm_lock == nullptr) {
        return true;
    }
    return esp_pm_lock_release(g_platform_idle_pm_lock) == ESP_OK;
#else
    return true;
#endif
}

bool platform_light_sleep_for(uint32_t duration_ms)
{
    if (duration_ms == 0U) {
        return false;
    }

    const uint64_t sleep_us = (uint64_t)duration_ms * 1000ULL;
    if (esp_sleep_enable_timer_wakeup(sleep_us) != ESP_OK) {
        return false;
    }
    return esp_light_sleep_start() == ESP_OK;
}

PlatformStatus platform_uart_init(PlatformUartPort *port)
{
    (void)port;
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_uart_transmit(PlatformUartPort *port, uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    (void)port;
    (void)timeout_ms;
    return (Serial.write(data, size) == size) ? PLATFORM_STATUS_OK : PLATFORM_STATUS_ERROR;
}

PlatformStatus platform_uart_transmit_async(PlatformUartPort *port, uint8_t *data, uint16_t size)
{
    return platform_uart_transmit(port, data, size, 0U);
}

PlatformStatus platform_uart_receive_async(PlatformUartPort *port, uint8_t *data, uint16_t size)
{
    (void)port;
    (void)data;
    (void)size;
    return PLATFORM_STATUS_OK;
}

void platform_uart_irq_handler(PlatformUartPort *port)
{
    (void)port;
}

PlatformStatus platform_flash_unlock(void)
{
    return PLATFORM_STATUS_ERROR;
}

PlatformStatus platform_flash_lock(void)
{
    return PLATFORM_STATUS_ERROR;
}

PlatformStatus platform_flash_erase_pages(PlatformFlashErasePlan *plan, uint32_t *page_error)
{
    (void)plan;
    if (page_error != nullptr) {
        *page_error = 0U;
    }
    return PLATFORM_STATUS_ERROR;
}

PlatformStatus platform_flash_program_halfword(uint32_t program_type, uint32_t address, uint16_t value)
{
    (void)program_type;
    (void)address;
    (void)value;
    return PLATFORM_STATUS_ERROR;
}

void platform_irq_set_priority(int irq, uint32_t priority, uint32_t subpriority)
{
    (void)irq;
    (void)priority;
    (void)subpriority;
}

void platform_irq_enable(int irq)
{
    (void)irq;
}

void platform_irq_disable_all(void)
{
}

void platform_irq_enable_all(void)
{
}

uint32_t platform_cpu_hz(void)
{
    return getCpuFrequencyMhz() * 1000000UL;
}

bool platform_get_support_snapshot(PlatformSupportSnapshot *out)
{
    if (out == nullptr) {
        return false;
    }

    out->rtc_reset_domain_supported = true;
    out->rtc_wall_clock_supported = false;
    out->rtc_wall_clock_persistent = false;
    out->idle_light_sleep_supported = ESP32_IDLE_LIGHT_SLEEP_ENABLED != 0;
    out->watchdog_supported = false;
    out->flash_journal_supported = false;
    return true;
}

void platform_reset_system(void)
{
    ESP.restart();
}

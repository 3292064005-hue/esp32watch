#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include "esp32_port_config.h"
#include "platform_api.h"
#include "app_config.h"

extern "C" {
PlatformI2cBus platform_i2c_main = {};
PlatformRtcDevice platform_rtc_main = {};
PlatformAdcDevice platform_adc_main = {};
PlatformUartPort platform_uart_main = {};
PlatformWatchdog platform_watchdog_main = {};
PlatformGpioPort g_platform_gpioa = {0};
PlatformGpioPort g_platform_gpiob = {1};
}

static Preferences g_bkp_prefs;
static bool g_bkp_loaded = false;
static uint32_t g_bkp_regs[16] = {0};
static uint32_t g_adc_last_value = 2048U;

static int resolve_actual_gpio(PlatformGpioPort *port, uint16_t pin_mask)
{
    if (port == PLATFORM_GPIOB && pin_mask == PLATFORM_PIN_6) return ESP32_I2C_SCL_GPIO;
    if (port == PLATFORM_GPIOB && pin_mask == PLATFORM_PIN_7) return ESP32_I2C_SDA_GPIO;
    if (port == KEY_UP_GPIO_Port && pin_mask == KEY_UP_Pin) return ESP32_KEY_UP_GPIO;
    if (port == KEY_DOWN_GPIO_Port && pin_mask == KEY_DOWN_Pin) return ESP32_KEY_DOWN_GPIO;
    if (port == KEY_OK_GPIO_Port && pin_mask == KEY_OK_Pin) return ESP32_KEY_OK_GPIO;
    if (port == KEY_BACK_GPIO_Port && pin_mask == KEY_BACK_Pin) return ESP32_KEY_BACK_GPIO;
    if (port == BATTERY_ADC_GPIO_Port && pin_mask == BATTERY_ADC_Pin) return ESP32_BATTERY_ADC_GPIO;
    if (port == VIBE_GPIO_Port && pin_mask == VIBE_Pin) return ESP32_VIBE_GPIO;
    if (port == CHARGE_DET_GPIO_Port && pin_mask == CHARGE_DET_Pin) return ESP32_CHARGE_DET_GPIO;
    if (port == OLED_RESET_GPIO_Port && pin_mask == OLED_RESET_Pin) return ESP32_OLED_RESET_GPIO;
    return -1;
}

static void bkp_load_once(void)
{
    if (g_bkp_loaded) {
        return;
    }
    if (g_bkp_prefs.begin("watch_bkp", false)) {
        for (int i = 0; i < 16; ++i) {
            char key[8];
            snprintf(key, sizeof(key), "r%02d", i);
            g_bkp_regs[i] = g_bkp_prefs.getULong(key, 0U);
        }
        g_bkp_prefs.end();
    }
    g_bkp_loaded = true;
}

static void bkp_store_index(uint32_t index)
{
    if (index >= 16U) {
        return;
    }
    if (!g_bkp_prefs.begin("watch_bkp", false)) {
        return;
    }
    char key[8];
    snprintf(key, sizeof(key), "r%02lu", (unsigned long)index);
    g_bkp_prefs.putULong(key, g_bkp_regs[index]);
    g_bkp_prefs.end();
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
    (void)bus;
    if (ESP32_I2C_SDA_GPIO >= 0 && ESP32_I2C_SCL_GPIO >= 0) {
        Wire.begin(ESP32_I2C_SDA_GPIO, ESP32_I2C_SCL_GPIO);
    } else {
        Wire.begin();
    }
    Wire.setClock(ESP32_I2C_CLOCK_HZ);
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
    return PLATFORM_STATUS_OK;
}

void platform_rtc_backup_unlock(void)
{
}

uint32_t platform_rtc_backup_read(PlatformRtcDevice *device, uint32_t reg)
{
    (void)device;
    bkp_load_once();
    if (reg < 16U) {
        return g_bkp_regs[reg];
    }
    return 0U;
}

void platform_rtc_backup_write(PlatformRtcDevice *device, uint32_t reg, uint32_t value)
{
    (void)device;
    bkp_load_once();
    if (reg < 16U) {
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
    if (ESP32_BATTERY_ADC_GPIO >= 0) {
        g_adc_last_value = (uint32_t)analogRead(ESP32_BATTERY_ADC_GPIO);
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
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_watchdog_kick(PlatformWatchdog *device)
{
    (void)device;
    return PLATFORM_STATUS_OK;
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
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_flash_lock(void)
{
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_flash_erase_pages(PlatformFlashErasePlan *plan, uint32_t *page_error)
{
    (void)plan;
    if (page_error != nullptr) {
        *page_error = 0U;
    }
    return PLATFORM_STATUS_OK;
}

PlatformStatus platform_flash_program_halfword(uint32_t program_type, uint32_t address, uint16_t value)
{
    (void)program_type;
    (void)address;
    (void)value;
    return PLATFORM_STATUS_OK;
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

void platform_reset_system(void)
{
    ESP.restart();
}

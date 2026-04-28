#ifndef PLATFORM_API_H
#define PLATFORM_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLATFORM_STATUS_OK = 0U,
    PLATFORM_STATUS_ERROR = 1U
} PlatformStatus;

typedef enum {
    PLATFORM_PIN_LOW = 0U,
    PLATFORM_PIN_HIGH = 1U
} PlatformPinState;

typedef struct PlatformGpioPort PlatformGpioPort;
struct PlatformGpioPort {
    uint8_t port_id;
};

typedef struct {
    uint32_t pin_mask;
    uint32_t mode;
    uint32_t pull;
    uint32_t speed;
} PlatformGpioConfig;

typedef struct {
    void *instance;
    uint32_t clock_hz;
} PlatformI2cBus;

typedef struct {
    void *instance;
} PlatformRtcDevice;

typedef struct {
    void *instance;
    uint32_t channel;
} PlatformAdcDevice;

typedef struct {
    uint32_t channel;
    uint32_t rank;
    uint32_t sampling_time;
} PlatformAdcChannelConfig;

typedef struct {
    void *instance;
    uint32_t baud_rate;
} PlatformUartPort;

typedef struct {
    void *instance;
} PlatformWatchdog;

typedef struct {
    uint32_t type_erase;
    uint32_t page_address;
    uint32_t page_count;
} PlatformFlashErasePlan;

typedef struct {
    bool rtc_reset_domain_supported;
    bool rtc_wall_clock_supported;
    bool rtc_wall_clock_persistent;
    bool idle_light_sleep_supported;
    bool watchdog_supported;
    bool flash_journal_supported;
} PlatformSupportSnapshot;

/* Transitional field aliases used while old struct-member names are being removed. */
#define Pin pin_mask
#define Mode mode
#define Pull pull
#define Speed speed
#define Instance instance
#define Channel channel
#define Rank rank
#define SamplingTime sampling_time
#define TypeErase type_erase
#define PageAddress page_address
#define NbPages page_count

extern PlatformGpioPort g_platform_gpioa;
extern PlatformGpioPort g_platform_gpiob;
extern PlatformI2cBus platform_i2c_main;
extern PlatformRtcDevice platform_rtc_main;
extern PlatformAdcDevice platform_adc_main;
extern PlatformUartPort platform_uart_main;
extern PlatformWatchdog platform_watchdog_main;

#define PLATFORM_GPIOA (&g_platform_gpioa)
#define PLATFORM_GPIOB (&g_platform_gpiob)

#define PLATFORM_PIN_0   (1U << 0)
#define PLATFORM_PIN_1   (1U << 1)
#define PLATFORM_PIN_3   (1U << 3)
#define PLATFORM_PIN_5   (1U << 5)
#define PLATFORM_PIN_6   (1U << 6)
#define PLATFORM_PIN_7   (1U << 7)
#define PLATFORM_PIN_9   (1U << 9)
#define PLATFORM_PIN_10  (1U << 10)
#define PLATFORM_PIN_11  (1U << 11)
#define PLATFORM_PIN_14  (1U << 14)

#define PLATFORM_GPIO_MODE_OUTPUT_OD 0U
#define PLATFORM_GPIO_MODE_ANALOG 1U
#define PLATFORM_GPIO_MODE_OUTPUT_PP 2U
#define PLATFORM_GPIO_MODE_INPUT 3U

#define PLATFORM_GPIO_NO_PULL 0U
#define PLATFORM_GPIO_PULL_UP 1U

#define PLATFORM_GPIO_SPEED_LOW 0U
#define PLATFORM_GPIO_SPEED_HIGH 1U

#define PLATFORM_I2C_MEM_ADDR_8BIT 1U
#define I2C_MEMADD_SIZE_8BIT PLATFORM_I2C_MEM_ADDR_8BIT

#define PLATFORM_ADC_CHANNEL_0 0U
#define PLATFORM_FLASH_PROGRAM_HALFWORD 0U
#define PLATFORM_FLASH_ERASE_PAGES 0U

#define PLATFORM_I2C_INSTANCE_MAIN ((void *)1)
#define PLATFORM_ADC_INSTANCE_MAIN ((void *)2)
#define PLATFORM_WATCHDOG_INSTANCE_MAIN ((void *)3)
#define PLATFORM_UART_INSTANCE_MAIN ((void *)10)
#define PLATFORM_RTC_INSTANCE_MAIN ((void *)11)

#define PLATFORM_BKP_REG1 1U
#define PLATFORM_BKP_REG2 2U
#define PLATFORM_BKP_REG3 3U
#define PLATFORM_BKP_REG4 4U
#define PLATFORM_BKP_REG5 5U
#define PLATFORM_BKP_REG6 6U
#define PLATFORM_BKP_REG7 7U
#define PLATFORM_BKP_REG8 8U
#define PLATFORM_BKP_REG9 9U
#define PLATFORM_BKP_REG10 10U
#define PLATFORM_BKP_REG11 11U
#define PLATFORM_BKP_REG12 12U
#define PLATFORM_BKP_REG13 13U
#define PLATFORM_BKP_REG14 14U

void platform_init(void);

uint32_t platform_time_now_ms(void);
void platform_delay_ms(uint32_t delay_ms);

void platform_gpio_init(PlatformGpioPort *port, const PlatformGpioConfig *config);
PlatformPinState platform_gpio_read(PlatformGpioPort *port, uint16_t pin_mask);
void platform_gpio_write(PlatformGpioPort *port, uint16_t pin_mask, PlatformPinState state);

PlatformStatus platform_i2c_init(PlatformI2cBus *bus);
PlatformStatus platform_i2c_deinit(PlatformI2cBus *bus);
PlatformStatus platform_i2c_write(PlatformI2cBus *bus, uint16_t address, uint8_t *data, uint16_t size, uint32_t timeout_ms);
PlatformStatus platform_i2c_mem_read(PlatformI2cBus *bus, uint16_t address, uint16_t mem_address, uint16_t mem_address_size, uint8_t *data, uint16_t size, uint32_t timeout_ms);

PlatformStatus platform_rtc_init(PlatformRtcDevice *device);
bool platform_rtc_wall_clock_supported(PlatformRtcDevice *device);
bool platform_rtc_wall_clock_persistent(PlatformRtcDevice *device);
PlatformStatus platform_rtc_get_epoch(PlatformRtcDevice *device, uint32_t *out_epoch);
PlatformStatus platform_rtc_set_epoch(PlatformRtcDevice *device, uint32_t epoch);
void platform_rtc_backup_unlock(void);
uint32_t platform_rtc_backup_read(PlatformRtcDevice *device, uint32_t reg);
void platform_rtc_backup_write(PlatformRtcDevice *device, uint32_t reg, uint32_t value);

PlatformStatus platform_adc_init(PlatformAdcDevice *device);
PlatformStatus platform_adc_config_channel(PlatformAdcDevice *device, void *channel_config);
PlatformStatus platform_adc_calibrate(PlatformAdcDevice *device);
PlatformStatus platform_adc_start(PlatformAdcDevice *device);
PlatformStatus platform_adc_poll(PlatformAdcDevice *device, uint32_t timeout_ms);
uint32_t platform_adc_read(PlatformAdcDevice *device);
PlatformStatus platform_adc_stop(PlatformAdcDevice *device);

PlatformStatus platform_watchdog_init(PlatformWatchdog *device);
PlatformStatus platform_watchdog_kick(PlatformWatchdog *device);

PlatformStatus platform_uart_init(PlatformUartPort *port);
PlatformStatus platform_uart_transmit(PlatformUartPort *port, uint8_t *data, uint16_t size, uint32_t timeout_ms);
PlatformStatus platform_uart_transmit_async(PlatformUartPort *port, uint8_t *data, uint16_t size);
PlatformStatus platform_uart_receive_async(PlatformUartPort *port, uint8_t *data, uint16_t size);
void platform_uart_irq_handler(PlatformUartPort *port);

PlatformStatus platform_flash_unlock(void);
PlatformStatus platform_flash_lock(void);
PlatformStatus platform_flash_erase_pages(PlatformFlashErasePlan *plan, uint32_t *page_error);
PlatformStatus platform_flash_program_halfword(uint32_t program_type, uint32_t address, uint16_t value);

void platform_irq_set_priority(int irq, uint32_t priority, uint32_t subpriority);
void platform_irq_enable(int irq);
void platform_irq_disable_all(void);
void platform_irq_enable_all(void);

uint32_t platform_cpu_hz(void);
bool platform_get_support_snapshot(PlatformSupportSnapshot *out);
/**
 * @brief Enter platform-managed light sleep for a bounded idle interval.
 *
 * @param[in] duration_ms Requested sleep interval in milliseconds. Values of
 *            zero are treated as no-op and return false.
 * @return true when the platform entered light sleep and resumed normally;
 *         false when the interval was invalid or the platform rejected the
 *         request.
 * @throws None.
 * @boundary_behavior Implementations may clamp the requested interval to the
 *                    hardware-supported minimum/maximum window.
 */
bool platform_light_sleep_for(uint32_t duration_ms);

/**
 * @brief Report whether platform power-management locks are compiled in.
 *
 * @return true when the active platform can create/use IDF PM locks; false when
 *         the adapter is compiled as a no-op.
 * @throws None.
 */
bool platform_pm_lock_supported(void);

/**
 * @brief Acquire the platform idle-sleep lock.
 *
 * @param[in] owner Stable caller label for diagnostics. May be NULL.
 * @return true when the lock was acquired or the adapter is disabled; false
 *         when a compiled-in PM lock backend rejected the acquire operation.
 * @throws None.
 */
bool platform_pm_lock_acquire(const char *owner);

/**
 * @brief Release the platform idle-sleep lock.
 *
 * @param[in] owner Stable caller label for diagnostics. May be NULL.
 * @return true when the lock was released or the adapter is disabled; false
 *         when a compiled-in PM lock backend rejected the release operation.
 * @throws None.
 */
bool platform_pm_lock_release(const char *owner);

void platform_reset_system(void);

#ifdef __cplusplus
}
#endif

#endif

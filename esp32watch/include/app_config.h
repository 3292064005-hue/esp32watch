#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "platform_api.h"
#include <stdbool.h>
#include <stdint.h>
#include "board_features.h"
#include "board_manifest.h"
#include "app_defaults.h"
#include "app_tuning.h"
#include "app_limits.h"
#include "esp32_port_config.h"

/* ---------------- Display ---------------- */
#define OLED_WIDTH                      128
#define OLED_HEIGHT                     64
#define OLED_BUFFER_SIZE                (OLED_WIDTH * OLED_HEIGHT / 8)

#define OLED_I2C_HANDLE                 platform_i2c_main
#define OLED_I2C_ADDRESS_7BIT           0x3C
#define OLED_I2C_ADDRESS                (OLED_I2C_ADDRESS_7BIT << 1)
#define OLED_I2C_TIMEOUT_MS             20
#define OLED_I2C_CHUNK_BYTES            32
#define OLED_RESET_ENABLED              (ESP32_OLED_RESET_GPIO >= 0)
#define OLED_RESET_GPIO_Port            PLATFORM_GPIOB
#define OLED_RESET_Pin                  PLATFORM_PIN_0

/* ---------------- Key pins ---------------- */
#define KEY_UP_GPIO_Port                PLATFORM_GPIOB
#define KEY_UP_Pin                      PLATFORM_PIN_11
#define KEY_DOWN_GPIO_Port              PLATFORM_GPIOB
#define KEY_DOWN_Pin                    PLATFORM_PIN_1
#define KEY_OK_GPIO_Port                PLATFORM_GPIOA
#define KEY_OK_Pin                      PLATFORM_PIN_5
#define KEY_BACK_GPIO_Port              PLATFORM_GPIOA
#define KEY_BACK_Pin                    PLATFORM_PIN_3

/* ---------------- Optional battery input ---------------- */
#define BATTERY_ADC_HANDLE              platform_adc_main
#define BATTERY_ADC_CHANNEL             PLATFORM_ADC_CHANNEL_0
#define BATTERY_ADC_GPIO_Port           PLATFORM_GPIOA
#define BATTERY_ADC_Pin                 PLATFORM_PIN_0
#define BATTERY_DIVIDER_NUMERATOR       2U
#define BATTERY_DIVIDER_DENOMINATOR     1U
#define BATTERY_DIVIDER_RATIO           ((float)BATTERY_DIVIDER_NUMERATOR / (float)BATTERY_DIVIDER_DENOMINATOR)
#define BATTERY_FULL_MV                 4200
#define BATTERY_EMPTY_MV                3300
#define CHARGE_DET_GPIO_Port            PLATFORM_GPIOB
#define CHARGE_DET_Pin                  PLATFORM_PIN_14
#define CHARGE_DET_ENABLED              (ESP32_CHARGE_DET_GPIO >= 0)

/* ---------------- Vibration ---------------- */
#define VIBE_GPIO_Port                  PLATFORM_GPIOB
#define VIBE_Pin                        PLATFORM_PIN_0

/* ---------------- Passive buzzer ---------------- */
#define BUZZER_GPIO                     ESP32_BUZZER_GPIO

/* ---------------- MPU6050 ---------------- */
#define MPU6050_I2C_HANDLE              platform_i2c_main
#define MPU6050_I2C_ADDRESS_7BIT        0x68
#define MPU6050_I2C_ADDRESS             (MPU6050_I2C_ADDRESS_7BIT << 1)
#define MPU6050_TIMEOUT_MS              20

/* ---------------- Companion UART ---------------- */
#if APP_FEATURE_COMPANION_UART
#define COMPANION_UART_HANDLE            platform_uart_main
#define COMPANION_UART_BAUDRATE          115200U
#define COMPANION_UART_TX_TIMEOUT_MS     50U
#define COMPANION_UART_GPIO_Port         PLATFORM_GPIOA
#define COMPANION_UART_TX_Pin            PLATFORM_PIN_9
#define COMPANION_UART_RX_Pin            PLATFORM_PIN_10
#endif

/* ---------------- RTC backup layout ---------------- */
#define BACKUP_MAGIC                    0x5A77U
#define BKP_DR_MAGIC                    PLATFORM_BKP_REG1
#define BKP_DR_VERSION                  PLATFORM_BKP_REG2
#define BKP_DR_SETTINGS0                PLATFORM_BKP_REG3
#define BKP_DR_SETTINGS1                PLATFORM_BKP_REG4
#define BKP_DR_SETTINGS2                PLATFORM_BKP_REG5
#define BKP_DR_ALARM                    PLATFORM_BKP_REG6
#define BKP_DR_ALARM1                   PLATFORM_BKP_REG12
#define BKP_DR_ALARM2                   PLATFORM_BKP_REG13
#define BKP_DR_ALARM_META               PLATFORM_BKP_REG14
#define BKP_DR_SENSOR_CAL0              PLATFORM_BKP_REG7
#define BKP_DR_SENSOR_CAL1              PLATFORM_BKP_REG8
#define BKP_DR_SENSOR_CAL2              PLATFORM_BKP_REG9
#define BKP_DR_SENSOR_CAL3              PLATFORM_BKP_REG10
#define BKP_DR_CRC                      PLATFORM_BKP_REG11

#define APP_CLAMP(v, minv, maxv)        ((v) < (minv) ? (minv) : ((v) > (maxv) ? (maxv) : (v)))

#endif

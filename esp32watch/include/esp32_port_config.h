#ifndef ESP32_PORT_CONFIG_H
#define ESP32_PORT_CONFIG_H

/*
 * ESP32-S3 board mapping.
 *
 * Values set to -1 mean "unmapped". The port still compiles and the
 * corresponding function degrades safely. For I2C, when both pins are -1 the
 * code falls back to Wire.begin() with the board default pins.
 */

#ifndef ESP32_I2C_SDA_GPIO
#define ESP32_I2C_SDA_GPIO             -1
#endif
#ifndef ESP32_I2C_SCL_GPIO
#define ESP32_I2C_SCL_GPIO             -1
#endif
#ifndef ESP32_I2C_CLOCK_HZ
#define ESP32_I2C_CLOCK_HZ             400000UL
#endif
#ifndef ESP32_MPU_I2C_SDA_GPIO
#define ESP32_MPU_I2C_SDA_GPIO         -1
#endif
#ifndef ESP32_MPU_I2C_SCL_GPIO
#define ESP32_MPU_I2C_SCL_GPIO         -1
#endif
#ifndef ESP32_MPU_I2C_CLOCK_HZ
#define ESP32_MPU_I2C_CLOCK_HZ         400000UL
#endif
#ifndef ESP32_KEY_UP_GPIO
#define ESP32_KEY_UP_GPIO              -1
#endif
#ifndef ESP32_KEY_DOWN_GPIO
#define ESP32_KEY_DOWN_GPIO            -1
#endif
#ifndef ESP32_KEY_OK_GPIO
#define ESP32_KEY_OK_GPIO              0
#endif
#ifndef ESP32_KEY_BACK_GPIO
#define ESP32_KEY_BACK_GPIO            -1
#endif
#ifndef ESP32_BATTERY_ADC_GPIO
#define ESP32_BATTERY_ADC_GPIO         -1
#endif
#ifndef ESP32_VIBE_GPIO
#define ESP32_VIBE_GPIO                -1
#endif
#ifndef ESP32_BUZZER_GPIO
#define ESP32_BUZZER_GPIO              17
#endif
#ifndef ESP32_CHARGE_DET_GPIO
#define ESP32_CHARGE_DET_GPIO          -1
#endif
#ifndef ESP32_OLED_RESET_GPIO
#define ESP32_OLED_RESET_GPIO          -1
#endif
#ifndef ESP32_NEOPIXEL_GPIO
#define ESP32_NEOPIXEL_GPIO            48
#endif

#endif

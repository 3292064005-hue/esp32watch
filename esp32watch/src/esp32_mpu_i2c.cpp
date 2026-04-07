#include <Arduino.h>
#include <Wire.h>
#include "esp32_mpu_i2c.h"
#include "esp32_port_config.h"

static TwoWire g_mpu_wire(1);
static bool g_mpu_wire_ready = false;

static bool esp32_mpu_i2c_begin_bus(void)
{
#if (ESP32_MPU_I2C_SDA_GPIO >= 0) && (ESP32_MPU_I2C_SCL_GPIO >= 0)
    g_mpu_wire.begin(ESP32_MPU_I2C_SDA_GPIO, ESP32_MPU_I2C_SCL_GPIO);
    g_mpu_wire.setClock(ESP32_MPU_I2C_CLOCK_HZ);
    g_mpu_wire_ready = true;
    return true;
#else
    g_mpu_wire_ready = false;
    return false;
#endif
}

extern "C" bool esp32_mpu_i2c_available(void)
{
#if (ESP32_MPU_I2C_SDA_GPIO >= 0) && (ESP32_MPU_I2C_SCL_GPIO >= 0)
    return true;
#else
    return false;
#endif
}

extern "C" bool esp32_mpu_i2c_init(void)
{
    if (!esp32_mpu_i2c_available()) {
        return false;
    }
    return esp32_mpu_i2c_begin_bus();
}

extern "C" bool esp32_mpu_i2c_recover_bus(void)
{
    if (!esp32_mpu_i2c_available()) {
        return false;
    }

    g_mpu_wire_ready = false;
    pinMode(ESP32_MPU_I2C_SCL_GPIO, OUTPUT_OPEN_DRAIN);
    pinMode(ESP32_MPU_I2C_SDA_GPIO, OUTPUT_OPEN_DRAIN);
    digitalWrite(ESP32_MPU_I2C_SCL_GPIO, HIGH);
    digitalWrite(ESP32_MPU_I2C_SDA_GPIO, HIGH);
    delay(1);

    for (uint8_t i = 0; i < 9U; ++i) {
        digitalWrite(ESP32_MPU_I2C_SCL_GPIO, LOW);
        delay(1);
        digitalWrite(ESP32_MPU_I2C_SCL_GPIO, HIGH);
        delay(1);
        if (digitalRead(ESP32_MPU_I2C_SDA_GPIO) == HIGH) {
            break;
        }
    }

    digitalWrite(ESP32_MPU_I2C_SDA_GPIO, LOW);
    delay(1);
    digitalWrite(ESP32_MPU_I2C_SCL_GPIO, HIGH);
    delay(1);
    digitalWrite(ESP32_MPU_I2C_SDA_GPIO, HIGH);
    delay(1);

    return esp32_mpu_i2c_begin_bus();
}

extern "C" PlatformStatus esp32_mpu_i2c_write(uint8_t address_7bit,
                                                 const uint8_t *data,
                                                 uint16_t size,
                                                 uint32_t timeout_ms)
{
    uint32_t start_ms = millis();

    (void)timeout_ms;
    if (data == nullptr || size == 0U) {
        return PLATFORM_STATUS_ERROR;
    }
    if (!g_mpu_wire_ready && !esp32_mpu_i2c_begin_bus()) {
        return PLATFORM_STATUS_ERROR;
    }

    g_mpu_wire.beginTransmission(address_7bit);
    size_t written = g_mpu_wire.write(data, size);
    uint8_t err = g_mpu_wire.endTransmission();
    if (written == size && err == 0U) {
        return PLATFORM_STATUS_OK;
    }

    if ((millis() - start_ms) <= timeout_ms && esp32_mpu_i2c_recover_bus()) {
        g_mpu_wire.beginTransmission(address_7bit);
        written = g_mpu_wire.write(data, size);
        err = g_mpu_wire.endTransmission();
        if (written == size && err == 0U) {
            return PLATFORM_STATUS_OK;
        }
    }
    return PLATFORM_STATUS_ERROR;
}

extern "C" PlatformStatus esp32_mpu_i2c_read_reg(uint8_t address_7bit,
                                                    uint8_t reg,
                                                    uint8_t *data,
                                                    uint16_t size,
                                                    uint32_t timeout_ms)
{
    uint32_t start_ms = millis();

    (void)timeout_ms;
    if (data == nullptr || size == 0U) {
        return PLATFORM_STATUS_ERROR;
    }
    if (!g_mpu_wire_ready && !esp32_mpu_i2c_begin_bus()) {
        return PLATFORM_STATUS_ERROR;
    }

    g_mpu_wire.beginTransmission(address_7bit);
    g_mpu_wire.write(reg);
    if (g_mpu_wire.endTransmission(false) == 0U) {
        size_t requested = g_mpu_wire.requestFrom((int)address_7bit, (int)size);
        if (requested == size) {
            for (uint16_t i = 0; i < size; ++i) {
                data[i] = (uint8_t)g_mpu_wire.read();
            }
            return PLATFORM_STATUS_OK;
        }
    }

    if ((millis() - start_ms) <= timeout_ms && esp32_mpu_i2c_recover_bus()) {
        g_mpu_wire.beginTransmission(address_7bit);
        g_mpu_wire.write(reg);
        if (g_mpu_wire.endTransmission(false) == 0U) {
            size_t requested = g_mpu_wire.requestFrom((int)address_7bit, (int)size);
            if (requested == size) {
                for (uint16_t i = 0; i < size; ++i) {
                    data[i] = (uint8_t)g_mpu_wire.read();
                }
                return PLATFORM_STATUS_OK;
            }
        }
    }

    return PLATFORM_STATUS_ERROR;
}


#include "drv_mpu6050.h"
#include "app_config.h"
#include "board_manifest.h"
#include "bsp_i2c.h"
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
#include "esp32_mpu_i2c.h"
#endif

#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_WHOAMI_VALUE     0x68U


static const BoardManifest *mpu_manifest(void)
{
    return board_manifest_get();
}

DrvMpu6050Status drv_mpu6050_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t packet[2] = {reg, value};
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    if (!esp32_mpu_i2c_available()) {
        return DRV_MPU6050_STATUS_INVALID_ARG;
    }
    return esp32_mpu_i2c_write(MPU6050_I2C_ADDRESS_7BIT,
                               packet,
                               2U,
                               MPU6050_TIMEOUT_MS) == PLATFORM_STATUS_OK
               ? DRV_MPU6050_STATUS_OK
               : DRV_MPU6050_STATUS_IO;
#else
    const BoardManifest *manifest = mpu_manifest();
    if (manifest == NULL || manifest->i2c_port == NULL) {
        return DRV_MPU6050_STATUS_INVALID_ARG;
    }
    return bsp_i2c_master_transmit_recoverable(&MPU6050_I2C_HANDLE,
                                               manifest->i2c_port,
                                               manifest->i2c_scl_pin,
                                               manifest->i2c_sda_pin,
                                               MPU6050_I2C_ADDRESS,
                                               packet,
                                               2U,
                                               MPU6050_TIMEOUT_MS) == PLATFORM_STATUS_OK
               ? DRV_MPU6050_STATUS_OK
               : DRV_MPU6050_STATUS_IO;
#endif
}

DrvMpu6050Status drv_mpu6050_read_reg(uint8_t reg, uint8_t *value)
{
    if (value == NULL) {
        return DRV_MPU6050_STATUS_INVALID_ARG;
    }
    return drv_mpu6050_read_block(reg, value, 1U);
}

DrvMpu6050Status drv_mpu6050_read_block(uint8_t reg, uint8_t *buf, uint16_t len)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    if (buf == NULL || len == 0U || !esp32_mpu_i2c_available()) {
        return DRV_MPU6050_STATUS_INVALID_ARG;
    }
    return esp32_mpu_i2c_read_reg(MPU6050_I2C_ADDRESS_7BIT,
                                  reg,
                                  buf,
                                  len,
                                  MPU6050_TIMEOUT_MS) == PLATFORM_STATUS_OK
               ? DRV_MPU6050_STATUS_OK
               : DRV_MPU6050_STATUS_IO;
#else
    const BoardManifest *manifest = mpu_manifest();
    if (buf == NULL || len == 0U || manifest == NULL || manifest->i2c_port == NULL) {
        return DRV_MPU6050_STATUS_INVALID_ARG;
    }
    return bsp_i2c_mem_read_recoverable(&MPU6050_I2C_HANDLE,
                                        manifest->i2c_port,
                                        manifest->i2c_scl_pin,
                                        manifest->i2c_sda_pin,
                                        MPU6050_I2C_ADDRESS,
                                        reg,
                                        I2C_MEMADD_SIZE_8BIT,
                                        buf,
                                        len,
                                        MPU6050_TIMEOUT_MS) == PLATFORM_STATUS_OK
               ? DRV_MPU6050_STATUS_OK
               : DRV_MPU6050_STATUS_IO;
#endif
}

DrvMpu6050Status drv_mpu6050_selftest(void)
{
    uint8_t who = 0U;
    DrvMpu6050Status st = drv_mpu6050_read_reg(MPU6050_REG_WHO_AM_I, &who);
    if (st != DRV_MPU6050_STATUS_OK) {
        return st;
    }
    return ((who & 0x7EU) == MPU6050_WHOAMI_VALUE) ? DRV_MPU6050_STATUS_OK : DRV_MPU6050_STATUS_BAD_WHOAMI;
}

DrvMpu6050Status drv_mpu6050_configure_default(void)
{
    if (drv_mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00U) != DRV_MPU6050_STATUS_OK) {
        return DRV_MPU6050_STATUS_IO;
    }
    platform_delay_ms(5U);
    if (drv_mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 0x13U) != DRV_MPU6050_STATUS_OK) {
        return DRV_MPU6050_STATUS_IO;
    }
    if (drv_mpu6050_write_reg(MPU6050_REG_CONFIG, 0x03U) != DRV_MPU6050_STATUS_OK) {
        return DRV_MPU6050_STATUS_IO;
    }
    if (drv_mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, 0x00U) != DRV_MPU6050_STATUS_OK) {
        return DRV_MPU6050_STATUS_IO;
    }
    if (drv_mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00U) != DRV_MPU6050_STATUS_OK) {
        return DRV_MPU6050_STATUS_IO;
    }
    return DRV_MPU6050_STATUS_OK;
}

uint8_t drv_mpu6050_status_to_error_code(DrvMpu6050Status status, uint8_t fallback_error)
{
    switch (status) {
        case DRV_MPU6050_STATUS_OK: return 0U;
        case DRV_MPU6050_STATUS_BAD_WHOAMI: return 2U;
        case DRV_MPU6050_STATUS_INVALID_ARG: return 9U;
        case DRV_MPU6050_STATUS_IO:
        default:
            return fallback_error;
    }
}


#include "sensor_internal.h"
#include "board_manifest.h"
#include "bsp_i2c.h"
#include "drv_mpu6050.h"
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
#include "esp32_mpu_i2c.h"
#endif


static const BoardManifest *sensor_manifest(void)
{
    return board_manifest_get();
}

void sensor_i2c_config_output_od(void)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    (void)esp32_mpu_i2c_init();
#else
    const BoardManifest *manifest = sensor_manifest();
    bsp_i2c_config_output_od(manifest->i2c_port, manifest->i2c_scl_pin, manifest->i2c_sda_pin);
#endif
}

void sensor_i2c_config_peripheral(void)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    (void)esp32_mpu_i2c_init();
#else
    const BoardManifest *manifest = sensor_manifest();
    bsp_i2c_config_peripheral(manifest->i2c_port, manifest->i2c_scl_pin, manifest->i2c_sda_pin);
#endif
}

void sensor_recover_i2c_bus(void)
{
#if defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
    (void)esp32_mpu_i2c_recover_bus();
#else
    const BoardManifest *manifest = sensor_manifest();
    (void)bsp_i2c_recover_bus(&MPU6050_I2C_HANDLE,
                              manifest->i2c_port,
                              manifest->i2c_scl_pin,
                              manifest->i2c_sda_pin);
#endif
}

bool mpu_write(uint8_t reg, uint8_t value)
{
    return drv_mpu6050_write_reg(reg, value) == DRV_MPU6050_STATUS_OK;
}

bool mpu_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return drv_mpu6050_read_block(reg, buf, len) == DRV_MPU6050_STATUS_OK;
}

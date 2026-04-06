#ifndef DRV_MPU6050_H
#define DRV_MPU6050_H

#include <stdint.h>
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRV_MPU6050_STATUS_OK = 0,
    DRV_MPU6050_STATUS_IO,
    DRV_MPU6050_STATUS_BAD_WHOAMI,
    DRV_MPU6050_STATUS_INVALID_ARG
} DrvMpu6050Status;

DrvMpu6050Status drv_mpu6050_write_reg(uint8_t reg, uint8_t value);
DrvMpu6050Status drv_mpu6050_read_reg(uint8_t reg, uint8_t *value);
DrvMpu6050Status drv_mpu6050_read_block(uint8_t reg, uint8_t *buf, uint16_t len);
DrvMpu6050Status drv_mpu6050_selftest(void);
DrvMpu6050Status drv_mpu6050_configure_default(void);
uint8_t drv_mpu6050_status_to_error_code(DrvMpu6050Status status, uint8_t fallback_error);

#ifdef __cplusplus
}
#endif

#endif

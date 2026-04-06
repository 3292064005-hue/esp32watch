# ESP32-S3 Watch Port (from STM32 firmware)

## What was migrated

This project keeps the original STM32 watch firmware application layers:
- model / storage logic
- UI routing and page logic
- display composition and SSD1306 protocol path
- sensor runtime state machine and MPU6050 driver
- alarm / activity / diagnostic services

The following layers were replaced for ESP32-S3:
- MCU startup and board bring-up
- HAL compatibility layer
- RTC/time backend
- backup-register persistence backend (mapped to NVS)
- reset-reason mapping

## Default feature state in this port

- Sensor: enabled
- Companion UART: disabled
- Watchdog: disabled
- Battery ADC: disabled
- Vibration motor: disabled
- Flash page journal backend: disabled
- Settings/alarm/calibration persistence: enabled through NVS-backed backup-register emulation

## Critical pin configuration

Edit `include/esp32_port_config.h` before flashing if your board pins differ.

Important defaults:
- `ESP32_I2C_SDA_GPIO = 8`
- `ESP32_I2C_SCL_GPIO = 9`
- `ESP32_KEY_OK_GPIO = 0`
- other keys default to `-1`

On this board wiring, the SSD1306 and MPU6050 share I2C on GPIO8/9.

## Known limits of this migration

1. I could not run a true PlatformIO target build in the container because PlatformIO/Arduino toolchains were not installed here.
2. This port is structured to preserve the original firmware logic, but final pin binding and board-specific validation still must be done on the real ESP32-S3 hardware.
3. Companion UART transport was left disabled because the original implementation is STM32 interrupt-oriented and the upload did not include the actual ESP32 serial transport contract you want.
4. Button mapping is intentionally conservative; unmapped keys degrade to "not pressed" instead of causing false input.

## Flashing workflow

Use `.\flash_all.ps1` from the project root to upload the LittleFS image first and then the firmware for the default ESP32-S3 N16R8 environment.

## Recommended next validation order

1. Confirm OLED works on your board.
2. Confirm time/watchface renders.
3. Confirm one button path works (`OK` defaults to GPIO0).
4. Confirm MPU6050 is present on the same I2C bus.
5. Then decide whether you want battery / vibration / UART features re-enabled.

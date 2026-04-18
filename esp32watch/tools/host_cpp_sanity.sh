#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
STUB_DIR="tools/host_stubs"
COMMON_DEFINES=(
  -DAPP_BOARD_PROFILE_ESP32S3_WATCH
  -DESP32_I2C_SDA_GPIO=8
  -DESP32_I2C_SCL_GPIO=9
  -DESP32_MPU_I2C_SDA_GPIO=4
  -DESP32_MPU_I2C_SCL_GPIO=5
  -DESP32_KEY_UP_GPIO=11
  -DESP32_KEY_DOWN_GPIO=1
  -DESP32_KEY_OK_GPIO=6
  -DESP32_KEY_BACK_GPIO=3
  -DESP32_IDLE_LIGHT_SLEEP_ENABLED=1
  -DESP32_IDLE_LIGHT_SLEEP_MIN_MS=2
  -DESP32_IDLE_LIGHT_SLEEP_ACTIVE_MS=6
  -DESP32_IDLE_LIGHT_SLEEP_UI_SLEEP_MS=40
  -DESP32_IDLE_LIGHT_SLEEP_MAX_MS=100
  -DWEB_PROVISIONING_SERIAL_PASSWORD_LOG=1
)
CPP_FILES=(
  src/main.cpp
  src/system_init_esp32.cpp
  src/persist.cpp
  src/persist_preferences.cpp
  src/services/device_config.cpp
  src/services/time_service.cpp
  src/services/network_sync_service.cpp
  src/web/web_server.cpp
  src/web/web_wifi.cpp
  src/web/web_state_bridge.cpp
  src/web/web_state_core.cpp
  src/web/web_state_detail.cpp
  src/web/web_action_queue.cpp
  src/web/web_routes_api.cpp
  src/web/web_routes_page.cpp
  src/web/web_json.cpp
  src/web/web_overlay.cpp
  src/web/web_runtime_snapshot.cpp
  src/platform_esp32s3.cpp
  src/display_backend.cpp
  src/esp32_mpu_i2c.cpp
  src/drv_buzzer.cpp
  src/reset_reason_esp32.cpp
)
for f in "${CPP_FILES[@]}"; do
  g++ -std=c++17 -fsyntax-only "${COMMON_DEFINES[@]}" -I"${STUB_DIR}" -Iinclude -Isrc "$f"
done
echo "[OK] host C++ stub syntax check passed for ${#CPP_FILES[@]} translation units"

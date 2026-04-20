#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
out_dir="/tmp/esp32watch_host_tests"
mkdir -p "$out_dir"

common=(
  -std=c11 -Iinclude -Isrc
  -DESP32_COMPANION_TX_GPIO=-1
  -DESP32_COMPANION_RX_GPIO=-1
  tools/host_tests/test_board_profile_matrix.c
  src/board_manifest.c
)

gcc "${common[@]}" \
  -DAPP_BOARD_PROFILE_ESP32S3_WATCH \
  -DESP32_I2C_SDA_GPIO=8 \
  -DESP32_I2C_SCL_GPIO=9 \
  -DESP32_MPU_I2C_SDA_GPIO=4 \
  -DESP32_MPU_I2C_SCL_GPIO=5 \
  -DESP32_KEY_UP_GPIO=11 \
  -DESP32_KEY_DOWN_GPIO=1 \
  -DESP32_KEY_OK_GPIO=6 \
  -DESP32_KEY_BACK_GPIO=3 \
  -o "$out_dir/board_profile_matrix_esp32"
"$out_dir/board_profile_matrix_esp32"

gcc "${common[@]}" -DAPP_BOARD_PROFILE_BLUEPILL_BATTERY -o "$out_dir/board_profile_matrix_bluepill_battery"
"$out_dir/board_profile_matrix_bluepill_battery"

gcc "${common[@]}" -DAPP_BOARD_PROFILE_BLUEPILL_FULL -o "$out_dir/board_profile_matrix_bluepill_full"
"$out_dir/board_profile_matrix_bluepill_full"

gcc "${common[@]}" -DAPP_BOARD_PROFILE_BLUEPILL_CORE -o "$out_dir/board_profile_matrix_bluepill_core"
"$out_dir/board_profile_matrix_bluepill_core"

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v pio >/dev/null 2>&1; then
  echo "[ERROR] pio not found" >&2
  exit 1
fi

if [[ $# -gt 0 ]]; then
  envs=("$@")
else
  envs=(esp32s3_n16r8_dev esp32s3_n16r8_release)
fi

for env in "${envs[@]}"; do
  echo "[INFO] Building firmware for ${env}"
  pio run -e "${env}"
  [[ -f ".pio/build/${env}/firmware.bin" ]]
  echo "[INFO] Building LittleFS image for ${env}"
  pio run -e "${env}" -t buildfs
  [[ -f ".pio/build/${env}/littlefs.bin" ]]
  echo "[OK] ${env} produced firmware.bin and littlefs.bin"
 done

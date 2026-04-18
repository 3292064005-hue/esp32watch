#!/usr/bin/env bash
set -euo pipefail

ENV_NAME="${1:-esp32s3_n16r8_dev}"
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

if ! command -v pio >/dev/null 2>&1; then
  echo "[ERROR] PlatformIO (pio) not found in PATH" >&2
  exit 1
fi

echo "[INFO] Building firmware for ${ENV_NAME}"
pio run -e "$ENV_NAME"

echo "[INFO] Building LittleFS assets for ${ENV_NAME}"
pio run -e "$ENV_NAME" -t buildfs

echo "[INFO] Uploading firmware for ${ENV_NAME}"
pio run -e "$ENV_NAME" -t upload

echo "[INFO] Uploading LittleFS assets for ${ENV_NAME}"
pio run -e "$ENV_NAME" -t uploadfs

echo "[INFO] Flash workflow completed for ${ENV_NAME}"

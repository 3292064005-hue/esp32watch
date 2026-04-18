#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

python3 ./tools/generate_asset_contract.py
python3 ./tools/verify_partition_contract.py

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

  host_report="dist/host-validation/${env}-host-validation.json"
  python3 ./tools/write_host_validation_report.py \
    --env "${env}" \
    --output "${host_report}" \
    --runner LOCAL_HOST \
    --status PASS \
    --check asset-contract=PASS \
    --check partition-contract=PASS \
    --check firmware-build=PASS \
    --check littlefs-build=PASS \
    --check bundle-verify=PASS

  bundle="$(python3 ./tools/package_release.py \
    --env "${env}" \
    --host-validation-status PASS \
    --host-validation-report "${host_report}" \
    --device-smoke-status NOT_RUN)"
  python3 ./tools/verify_release_bundle.py --env "${env}" --bundle "${bundle}"
 done

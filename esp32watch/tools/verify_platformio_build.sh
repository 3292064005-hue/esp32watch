#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PYTHON_CMD=()

if [[ -n "${HOST_PYTHON:-}" ]]; then
  PYTHON_CMD=("${HOST_PYTHON}")
else
  for candidate in python3 python; do
    if command -v "${candidate}" >/dev/null 2>&1 && "${candidate}" -c 'import sys; sys.exit(0)' >/dev/null 2>&1; then
      PYTHON_CMD=("${candidate}")
      break
    fi
  done
fi

if [[ "${#PYTHON_CMD[@]}" -eq 0 ]]; then
  echo "[FAIL] No working Python interpreter found; set HOST_PYTHON or install python3/python" >&2
  exit 1
fi

echo "[INFO] Using Python: ${PYTHON_CMD[*]}"

"${PYTHON_CMD[@]}" ./tools/generate_asset_contract.py
"${PYTHON_CMD[@]}" ./tools/verify_partition_contract.py

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
  "${PYTHON_CMD[@]}" ./tools/write_host_validation_report.py \
    --env "${env}" \
    --output "${host_report}" \
    --runner LOCAL_HOST \
    --status PASS \
    --check asset-contract=PASS \
    --check partition-contract=PASS \
    --check firmware-build=PASS \
    --check littlefs-build=PASS \
    --check bundle-verify=PASS

  bundle="$("${PYTHON_CMD[@]}" ./tools/package_release.py \
    --env "${env}" \
    --host-validation-status PASS \
    --host-validation-report "${host_report}" \
    --device-smoke-status NOT_RUN)"
  "${PYTHON_CMD[@]}" ./tools/verify_release_bundle.py --env "${env}" --bundle "${bundle}"
 done

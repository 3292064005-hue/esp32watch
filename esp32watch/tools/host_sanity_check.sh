#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
trap 'rm -rf .pio dist .pytest_cache' EXIT

HOST_TOOL_TIMEOUT_SEC="${HOST_TOOL_TIMEOUT_SEC:-300}"
HOST_COMPILE_TIMEOUT_SEC="${HOST_COMPILE_TIMEOUT_SEC:-600}"

run_step() {
  local label="$1"
  shift
  echo "[RUN] ${label}"
  if command -v timeout >/dev/null 2>&1; then
    timeout "${HOST_TOOL_TIMEOUT_SEC}" "$@"
  else
    "$@"
  fi
}

run_compile_step() {
  local label="$1"
  shift
  echo "[RUN] ${label}"
  if command -v timeout >/dev/null 2>&1; then
    timeout "${HOST_COMPILE_TIMEOUT_SEC}" "$@"
  else
    "$@"
  fi
}

run_step "host Python dependency check" python3 ./tools/verify_host_python_requirements.py
run_compile_step "asset contract + runtime bootstrap generation" python3 ./tools/generate_asset_contract.py
run_step "partition contract" python3 ./tools/verify_partition_contract.py
run_step "workflow contract" python3 ./tools/host_tests/test_workflow_yaml.py

if command -v node >/dev/null 2>&1; then
  run_step "web console syntax: app-core" node --check data/app-core.js
  run_step "web console syntax: app-render" node --check data/app-render.js
  run_step "web console syntax: app-actions" node --check data/app-actions.js
  run_step "web console syntax: app bootstrap" node --check data/app.js
  run_step "web runtime smoke" node ./tools/web_runtime_smoke.js
else
  echo "[WARN] node not found; skipped web-console syntax + smoke checks"
fi

c_count=0
while IFS= read -r f; do
  case "$f" in
    src/syscalls.c|src/legacy/*) continue ;;
  esac
  run_compile_step "C syntax: ${f}" gcc -std=c11 -fsyntax-only -Iinclude -Isrc "$f"
  c_count=$((c_count + 1))
done < <(find src -type f -name '*.c' | sort)
echo "[OK] host C syntax check passed for ${c_count} translation units"

run_compile_step "host C++ stub syntax" bash ./tools/host_cpp_sanity.sh
run_step "runtime contract static check" python3 ./tools/host_runtime_contract_check.py
run_step "runtime contract output parity" python3 ./tools/host_tests/test_runtime_contract_output.py
run_step "compatibility lifecycle" python3 ./tools/host_tests/test_compatibility_lifecycle.py
run_step "device-config authority boundary scan" python3 ./tools/host_tests/test_device_config_authority_boundaries.py
run_step "runtime reload/event manifest alignment" python3 ./tools/host_tests/test_runtime_reload_event_matrix.py

run_compile_step "runtime reload/event manifest host test" gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_runtime_reload_event_manifest.c \
  src/services/runtime_reload_event_manifest.c \
  -o /tmp/esp32watch_runtime_reload_event_manifest_test
run_step "runtime reload/event manifest executable" /tmp/esp32watch_runtime_reload_event_manifest_test

run_compile_step "companion proto contract host test" gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_companion_proto_contract.c \
  src/companion_proto_contract.c \
  src/companion_proto_parser.c \
  -o /tmp/esp32watch_companion_proto_contract_test
run_step "companion proto contract executable" /tmp/esp32watch_companion_proto_contract_test

run_step "change validation matrix" python3 ./tools/verify_change_validation_matrix.py
run_compile_step "host behavior check" bash ./tools/host_behavior_check.sh

if command -v pio >/dev/null 2>&1; then
  if [[ "${SKIP_PIO:-0}" == "1" ]]; then
    echo "[WARN] SKIP_PIO=1 set; skipped PlatformIO build validation"
  else
    timeout "${PIO_TIMEOUT_SEC:-900}" bash ./tools/verify_platformio_build.sh "${PIO_ENV_NAME:-esp32s3_n16r8_dev}"
  fi
else
  echo "[WARN] pio not found; skipped PlatformIO build validation (host sanity does not prove firmware compilation in this run)"
fi

echo "[INFO] Host sanity check completed."

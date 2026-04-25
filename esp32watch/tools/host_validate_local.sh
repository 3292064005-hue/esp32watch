#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p dist/reports
started_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
HOST_TOOL_TIMEOUT_SEC="${HOST_TOOL_TIMEOUT_SEC:-300}" \
HOST_COMPILE_TIMEOUT_SEC="${HOST_COMPILE_TIMEOUT_SEC:-600}" \
bash ./tools/host_sanity_check.sh
mkdir -p dist/reports
finished_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
cat > dist/reports/host-local-validation.json <<JSON
{
  "reportType": "HOST_LOCAL_VALIDATION_REPORT",
  "startedAtUtc": "${started_at}",
  "finishedAtUtc": "${finished_at}",
  "result": "PASS",
  "entrypoint": "tools/host_validate_local.sh",
  "hostToolTimeoutSec": ${HOST_TOOL_TIMEOUT_SEC:-300},
  "hostCompileTimeoutSec": ${HOST_COMPILE_TIMEOUT_SEC:-600},
  "checks": [
    "host Python dependency check",
    "asset-contract generation",
    "runtime contract parity",
    "workflow contract guard",
    "web runtime smoke",
    "host C/C++ syntax checks",
    "authority boundary scan",
    "reload-event manifest alignment",
    "reload-event manifest executable test",
    "companion protocol contract executable test",
    "host behavior checks",
    "optional PlatformIO build validation when pio is installed"
  ]
}
JSON
printf '[INFO] Wrote dist/reports/host-local-validation.json\n'

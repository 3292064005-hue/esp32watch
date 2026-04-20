#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
scenario="${1:-baseline}"
out_dir="/tmp/esp32watch_host_tests"
mkdir -p "$out_dir"

BIN="$out_dir/host_runtime_simulator_matrix"
OUT="$out_dir/host_runtime_simulator_${scenario}.json"

gcc -std=c11 -Iinclude -Isrc \
  tools/host_runtime_simulator.c \
  src/watch_app_runtime.c \
  src/watch_app_policy.c \
  src/ui_page_watchface.c \
  src/ui_page_activity.c \
  src/ui_page_storage.c \
  src/ui_page_about.c \
  src/display_core.c \
  src/display_draw.c \
  src/display_widgets.c \
  -o "$BIN"

case "$scenario" in
  baseline)
    "$BIN" \
      --step WATCHFACE:0 \
      --step ACTIVITY:16 \
      --step STORAGE:32 \
      --step ABOUT:48 \
      --output "$OUT"
    python3 tools/host_tests/test_host_runtime_simulator.py "$OUT"
    ;;
  reset-flow)
    "$BIN" \
      --step WATCHFACE:0 \
      --step STORAGE:16 \
      --step WATCHFACE:32 \
      --step ABOUT:48 \
      --output "$OUT"
    python3 - "$OUT" <<'PY'
import json, sys
payload = json.load(open(sys.argv[1], 'r', encoding='utf-8'))
frames = payload['frames']
assert [f['page'] for f in frames] == ['WATCHFACE', 'STORAGE', 'WATCHFACE', 'ABOUT']
assert frames[1]['frameCrc32'] != frames[2]['frameCrc32']
assert payload['runtime']['currentPage'] == 'ABOUT'
print('[OK] host runtime simulator reset-flow matrix check passed')
PY
    ;;
  storage-focus)
    "$BIN" \
      --step STORAGE:0 \
      --step STORAGE:16 \
      --step ABOUT:32 \
      --step STORAGE:48 \
      --output "$OUT"
    python3 - "$OUT" <<'PY'
import json, sys
payload = json.load(open(sys.argv[1], 'r', encoding='utf-8'))
frames = payload['frames']
assert [f['page'] for f in frames] == ['STORAGE', 'STORAGE', 'ABOUT', 'STORAGE']
assert frames[0]['frameCrc32'] == frames[1]['frameCrc32']
assert frames[1]['frameCrc32'] != frames[2]['frameCrc32']
assert frames[2]['frameCrc32'] != frames[3]['frameCrc32']
print('[OK] host runtime simulator storage-focus matrix check passed')
PY
    ;;
  *)
    echo "unknown simulator matrix scenario: $scenario" >&2
    exit 2
    ;;
esac

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
STUB_DIR="tools/host_stubs"
CPP_FILES=(
  src/main.cpp
  src/system_init_esp32.cpp
  src/services/device_config.cpp
  src/services/network_sync_service.cpp
  src/web/web_server.cpp
  src/web/web_wifi.cpp
  src/web/web_state_bridge.cpp
  src/web/web_state_core.cpp
  src/web/web_state_detail.cpp
  src/web/web_action_queue.cpp
  src/web/web_routes_api.cpp
)
for f in "${CPP_FILES[@]}"; do
  g++ -std=c++17 -fsyntax-only -I"${STUB_DIR}" -Iinclude -Isrc "$f"
done
echo "[OK] host C++ stub syntax check passed for ${#CPP_FILES[@]} translation units"

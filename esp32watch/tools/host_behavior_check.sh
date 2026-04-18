#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /tmp/esp32watch_host_tests
COMMON_CPP_DEFINES=(
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

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_runtime_event_service.c \
  src/services/runtime_event_service.c \
  -o /tmp/esp32watch_host_tests/runtime_event_service_test
/tmp/esp32watch_host_tests/runtime_event_service_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_watch_app_policy.c \
  src/watch_app_policy.c \
  -o /tmp/esp32watch_host_tests/watch_app_policy_test
/tmp/esp32watch_host_tests/watch_app_policy_test

gcc -std=c11 -Iinclude -Isrc   tools/host_tests/test_model_runtime_split.c   src/model_runtime.c   -o /tmp/esp32watch_host_tests/model_runtime_split_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_runtime_reload_coordinator.c \
  src/services/runtime_reload_coordinator.c \
  src/services/runtime_event_service.c \
  -o /tmp/esp32watch_host_tests/runtime_reload_coordinator_test
/tmp/esp32watch_host_tests/runtime_reload_coordinator_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_reset_service.c \
  src/services/reset_service.c \
  src/services/runtime_event_service.c \
  -o /tmp/esp32watch_host_tests/reset_service_test
/tmp/esp32watch_host_tests/reset_service_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_storage_semantics.c \
  src/services/storage_semantics.c \
  -o /tmp/esp32watch_host_tests/storage_semantics_test
/tmp/esp32watch_host_tests/storage_semantics_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_storage_facade.c \
  src/services/storage_facade.c \
  src/services/storage_semantics.c \
  -o /tmp/esp32watch_host_tests/storage_facade_test
/tmp/esp32watch_host_tests/storage_facade_test

/tmp/esp32watch_host_tests/model_runtime_split_test



gcc -std=c11 -Iinclude -Isrc \
  src/services/runtime_event_service.c \
  -c -o /tmp/esp32watch_host_tests/runtime_event_service.o

gcc -std=c11 "${COMMON_CPP_DEFINES[@]}" -Iinclude -Isrc \
  src/board_manifest.c \
  -c -o /tmp/esp32watch_host_tests/board_manifest.o

g++ -std=c++17 -DHOST_RUNTIME_TEST "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_runtime_cpp_services.cpp \
  src/web/web_wifi.cpp \
  src/services/network_sync_service.cpp \
  /tmp/esp32watch_host_tests/runtime_event_service.o \
  -o /tmp/esp32watch_host_tests/runtime_cpp_services_test
/tmp/esp32watch_host_tests/runtime_cpp_services_test

g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_platform_cpp_descriptor.cpp \
  src/platform_esp32s3.cpp \
  src/esp32_mpu_i2c.cpp \
  src/display_backend.cpp \
  /tmp/esp32watch_host_tests/board_manifest.o \
  -o /tmp/esp32watch_host_tests/platform_cpp_descriptor_test
/tmp/esp32watch_host_tests/platform_cpp_descriptor_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_ui_page_golden.c \
  src/ui_page_watchface.c \
  src/ui_page_activity.c \
  src/ui_page_storage.c \
  src/ui_page_system.c \
  src/display_core.c \
  src/display_draw.c \
  src/display_widgets.c \
  -o /tmp/esp32watch_host_tests/ui_page_golden_test
/tmp/esp32watch_host_tests/ui_page_golden_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_ui_page_sequence.c \
  src/ui_page_watchface.c \
  src/ui_page_activity.c \
  src/ui_page_storage.c \
  src/ui_page_about.c \
  src/display_core.c \
  src/display_draw.c \
  src/display_widgets.c \
  -o /tmp/esp32watch_host_tests/ui_page_sequence_test
/tmp/esp32watch_host_tests/ui_page_sequence_test

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
  -o /tmp/esp32watch_host_tests/host_runtime_simulator
/tmp/esp32watch_host_tests/host_runtime_simulator \
  --step WATCHFACE:0 \
  --step ACTIVITY:16 \
  --step STORAGE:32 \
  --step ABOUT:48 \
  --output /tmp/esp32watch_host_tests/host_runtime_simulator.json
python3 tools/host_tests/test_host_runtime_simulator.py /tmp/esp32watch_host_tests/host_runtime_simulator.json

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_ui_app_catalog.c \
  src/ui_app_catalog.c \
  -o /tmp/esp32watch_host_tests/ui_app_catalog_test
/tmp/esp32watch_host_tests/ui_app_catalog_test


gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_ui_page_catalog.c \
  src/ui_page_catalog.c \
  src/ui_page_registry.c \
  -o /tmp/esp32watch_host_tests/ui_page_catalog_test
/tmp/esp32watch_host_tests/ui_page_catalog_test

g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_web_state_core_startup.cpp \
  src/web/web_state_core.cpp \
  -o /tmp/esp32watch_host_tests/web_state_core_startup_test
/tmp/esp32watch_host_tests/web_state_core_startup_test


g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_time_service_faults.cpp \
  src/services/time_service.cpp \
  src/persist_preferences.cpp \
  -o /tmp/esp32watch_host_tests/time_service_faults_test
/tmp/esp32watch_host_tests/time_service_faults_test

python3 tools/host_tests/test_release_bundle.py
python3 tools/host_tests/test_capture_device_smoke_report.py
python3 tools/host_tests/test_run_device_scenarios.py
python3 tools/host_tests/test_flash_candidate_bundle.py
python3 tools/host_tests/test_runtime_contract_output.py

echo "[INFO] Host behavior checks completed."

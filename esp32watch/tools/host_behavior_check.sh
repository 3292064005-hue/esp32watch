#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /tmp/esp32watch_host_tests
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

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_model_runtime_split.c \
  src/model_runtime.c \
  src/model.c \
  src/model_alarm.c \
  src/model_activity.c \
  src/model_settings.c \
  -o /tmp/esp32watch_host_tests/model_runtime_split_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_runtime_reload_coordinator.c \
  src/services/runtime_reload_coordinator.c \
  src/services/runtime_event_service.c \
  -o /tmp/esp32watch_host_tests/runtime_reload_coordinator_test
/tmp/esp32watch_host_tests/runtime_reload_coordinator_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_runtime_side_effect_service.c \
  src/services/runtime_side_effect_service.c \
  -o /tmp/esp32watch_host_tests/runtime_side_effect_service_test
/tmp/esp32watch_host_tests/runtime_side_effect_service_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_reset_service.c \
  src/services/reset_service.c \
  src/services/runtime_side_effect_service.c \
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
  src/services/network_sync_runtime.cpp \
  src/services/network_sync_config.cpp \
  src/services/network_sync_time.cpp \
  src/services/network_sync_weather.cpp \
  src/services/network_sync_worker.cpp \
  src/services/network_sync_api.cpp \
  src/services/network_sync_codec.cpp \
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
"${PYTHON_CMD[@]}" tools/host_tests/test_host_runtime_simulator.py /tmp/esp32watch_host_tests/host_runtime_simulator.json
bash ./tools/run_host_simulator_matrix.sh baseline
bash ./tools/run_host_simulator_matrix.sh reset-flow
bash ./tools/run_host_simulator_matrix.sh storage-focus

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_ui_app_catalog.c \
  src/ui_app_catalog.c \
  -o /tmp/esp32watch_host_tests/ui_app_catalog_test
/tmp/esp32watch_host_tests/ui_app_catalog_test


gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_ui_page_catalog.c \
  src/ui_page_catalog.c \
  src/ui_page_registry.c \
  src/ui_page_module_registry.c \
  src/ui_page_module_core.c \
  src/ui_page_module_manifest.c \
  -o /tmp/esp32watch_host_tests/ui_page_catalog_test
/tmp/esp32watch_host_tests/ui_page_catalog_test

g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_web_state_core_startup.cpp \
  src/web/web_state_core.cpp \
  -o /tmp/esp32watch_host_tests/web_state_core_startup_test
/tmp/esp32watch_host_tests/web_state_core_startup_test


g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_system_runtime_service_init.cpp \
  src/system_init_esp32.cpp \
  src/system_init_stage.c \
  -o /tmp/esp32watch_host_tests/system_runtime_service_init_test
/tmp/esp32watch_host_tests/system_runtime_service_init_test

g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -DSYSTEM_RUNTIME_INIT_STORAGE_AUTHORITY_FIRST=0 -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_system_runtime_service_init.cpp \
  src/system_init_esp32.cpp \
  src/system_init_stage.c \
  -o /tmp/esp32watch_host_tests/system_runtime_service_init_legacy_test
/tmp/esp32watch_host_tests/system_runtime_service_init_legacy_test

g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_time_service_faults.cpp \
  src/services/time_service.cpp \
  src/persist_preferences.cpp \
  -o /tmp/esp32watch_host_tests/time_service_faults_test
/tmp/esp32watch_host_tests/time_service_faults_test

g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_time_service_authority_paths.cpp \
  src/services/time_service.cpp \
  src/persist_preferences.cpp \
  -o /tmp/esp32watch_host_tests/time_service_authority_paths_test
/tmp/esp32watch_host_tests/time_service_authority_paths_test

g++ -std=c++17 -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_web_asset_contract_parser.cpp \
  src/web/web_asset_contract_parser.cpp \
  -o /tmp/esp32watch_host_tests/web_asset_contract_parser_test
/tmp/esp32watch_host_tests/web_asset_contract_parser_test

g++ -std=c++17 "${COMMON_CPP_DEFINES[@]}" -Itools/host_stubs -Iinclude -Isrc \
  tools/host_tests/test_time_service_long_disconnect.cpp \
  src/services/time_service.cpp \
  src/persist_preferences.cpp \
  -o /tmp/esp32watch_host_tests/time_service_long_disconnect_test
/tmp/esp32watch_host_tests/time_service_long_disconnect_test

gcc -std=c11 -Iinclude -Isrc \
  tools/host_tests/test_model_alarm_time_integration.c \
  src/model.c \
  src/model_runtime.c \
  src/model_alarm.c \
  src/model_activity.c \
  src/model_settings.c \
  src/model_time.c \
  -o /tmp/esp32watch_host_tests/model_alarm_time_integration_test
/tmp/esp32watch_host_tests/model_alarm_time_integration_test

bash ./tools/run_host_board_profile_matrix.sh

gcc -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections -Iinclude -Isrc \
  tools/host_tests/test_command_catalog_compatibility.c \
  src/app_command.c \
  src/app_command_registry.c \
  src/app_command_module_core.c \
  src/app_command_module_manifest.c \
  -o /tmp/esp32watch_host_tests/command_catalog_compatibility_test
/tmp/esp32watch_host_tests/command_catalog_compatibility_test

"${PYTHON_CMD[@]}" tools/verify_host_python_requirements.py
"${PYTHON_CMD[@]}" tools/host_tests/test_compatibility_lifecycle.py
"${PYTHON_CMD[@]}" tools/host_tests/test_workflow_yaml.py

"${PYTHON_CMD[@]}" tools/host_tests/test_release_bundle.py
"${PYTHON_CMD[@]}" tools/host_tests/test_capture_device_smoke_report.py
"${PYTHON_CMD[@]}" tools/host_tests/test_run_device_scenarios.py
"${PYTHON_CMD[@]}" tools/host_tests/test_flash_candidate_bundle.py
"${PYTHON_CMD[@]}" tools/host_tests/test_runtime_contract_output.py

echo "[INFO] Host behavior checks completed."

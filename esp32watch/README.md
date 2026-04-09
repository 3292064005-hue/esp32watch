# ESP32-S3 Watch Runtime

## 1. Current project position

This repository is no longer just an STM32-to-ESP32 port note. The active runtime is already a composite system that includes:
- watch application main loop
- Wi-Fi provisioning + fallback AP
- web console and runtime control plane
- dual-layer persistence and migration status exposure
- sensor/runtime diagnostics
- network time + weather sync

The goal of this iteration is to make the **ESP32 active path authoritative**, tighten the build/runtime contract, and remove fake-closed semantics.

---

## 2. Active path vs legacy path

### Active ESP32 path
- `src/main.cpp`
- `src/system_init_esp32.cpp`
- `src/watch_app_runtime*.c`
- `src/platform_esp32s3.cpp`
- `src/display_backend.cpp`
- `src/services/time_service.cpp`
- `src/services/network_sync_service.cpp`
- `src/services/device_config.cpp`
- `src/services/storage_service.c`
- `src/web/*`

### Legacy isolated path
The following legacy implementations were moved out of the active build graph and are now archival/reference-only:
- `src/legacy/stm32/bsp_adc.c`
- `src/legacy/stm32/bsp_i2c.c`
- `src/legacy/stm32/drv_ssd1306.c`
- `src/legacy/services/time_service.c`

`platformio.ini` excludes `src/legacy/` from the authoritative ESP32 build so the repository no longer depends on silent source-tree dual-track behavior.

---

## 3. Supported build environments

`platformio.ini` now defines one base environment and two explicit run profiles:

- `esp32s3_n16r8_dev`
  - default environment
  - sleep disabled
  - intended for bring-up, web-console debugging, and active diagnostics
- `esp32s3_n16r8_release`
  - release-oriented profile
  - sleep path left enabled for product-style validation

The base environment pins the validated dependency set and embeds the weather TLS CA bundle into firmware resources.

---

## 4. Security and runtime semantics tightened in this revision

### 4.1 Weather TLS
Weather sync no longer pretends to be secure while calling `setInsecure()` by default.

Current behavior:
- embedded CA bundle present -> verified TLS path
- CA bundle missing and insecure mode disabled -> fail-closed, sync is rejected
- insecure mode is now an explicit build-time opt-in only

Exposed status fields now include:
- `weatherTlsVerified`
- `weatherCaLoaded`
- `weatherTlsMode`
- `weatherSyncStatus`
- `weatherLastHttpStatus`

### 4.2 LittleFS mount behavior
LittleFS mount failure is no longer destructive by default.

Current behavior:
- default: non-destructive mount failure reporting
- optional auto-format: build-time opt-in only
- web API now exposes:
  - `filesystemReady`
  - `filesystemAssetsReady`
  - `filesystemStatus`

### 4.3 Time authority
Time status now distinguishes source validity from source confidence.

Runtime exposure now includes:
- `timeSource`
- `timeConfidence`
- `timeValid`
- `timeAuthoritative`
- `timeSourceAgeMs`

This prevents the old recovery path from being mislabeled as a fake RTC-grade time authority.

### 4.4 API mutation contract
Write endpoints now accept structured JSON payloads and keep form compatibility where practical.

Main mutation endpoints:
- `POST /api/config/device`
- `POST /api/config/device/reset`
- `POST /api/input/key`
- `POST /api/command`
- `POST /api/display/overlay`
- `POST /api/display/overlay/clear`

Tracked mutation/result endpoints:
- `GET /api/actions/catalog`
- `GET /api/actions/latest`
- `GET /api/actions/status?id=<actionId>`
- legacy compatibility alias: `GET /api/action/result?id=<actionId>`

State exposure is now split by contract surface:
- `GET /api/state/summary`
- `GET /api/state/detail`
- `GET /api/state/raw`
- compatibility aggregate: `GET /api/state`

Mutation auth still uses `X-Auth-Token` and may also read a JSON `token` field for recovery workflows.

---

## 5. Web console behavior

The browser console is now aligned with the runtime contract instead of depending on ad-hoc form parsing only.

The UI now surfaces:
- time source confidence
- weather TLS mode / sync status
- filesystem readiness / asset status
- provisioning lock state
- API/state versioning

Runtime control remains locked while the device is in provisioning AP mode and no API token is configured.

---

## 6. Feature state snapshot

### Enabled / active
- sensor runtime
- NVS-backed configuration persistence
- web provisioning and web runtime control
- LittleFS-hosted web assets
- weather/time sync pipeline
- storage status observation

### Disabled / not yet productized
- companion UART transport
- watchdog path
- battery ADC path
- vibration motor path

### Profile dependent
- sleep / low-power runtime behavior
  - disabled in `dev`
  - available for validation in `release`

---

## 7. Board defaults

Edit `include/esp32_port_config.h` if your board wiring differs.

Default pins:
- display I2C SDA: GPIO8
- display I2C SCL: GPIO9
- MPU6050 SDA: GPIO4
- MPU6050 SCL: GPIO5
- OK key: GPIO0

---

## 8. Flashing workflow

Use PowerShell from the repository root:

```powershell
./flash_all.ps1
```

Default upload target:
- `esp32s3_n16r8_dev`

To flash another profile:

```powershell
./flash_all.ps1 -EnvName esp32s3_n16r8_release
```

The script uploads LittleFS first, then rebuilds and uploads firmware.

---

## 9. Validation status

### Already implemented in repo
- exact PlatformIO environment split
- active-path cleanup with `src/legacy/` isolation
- CA-bundled weather TLS path
- non-destructive LittleFS mount default
- time confidence/status exposure
- JSON mutation API contract
- web console alignment with new runtime fields
- tracked action queue/result contract
- summary/detail/raw state split
- storage backend naming aligned to NVS-backed emulation semantics

### Host-side verification available
Run:

```bash
./tools/host_sanity_check.sh
```

This performs:
- `data/app.js` syntax check when Node is available
- host C syntax check for portable C units
- host C++ stub syntax check for changed Arduino-facing units
- `./tools/verify_platformio_build.sh ${PIO_ENV_NAME:-esp32s3_n16r8_dev}` when PlatformIO is installed
- `./tools/host_runtime_contract_check.py` to statically verify startup gating, tracked actions, and split-state endpoint presence

### Additional build pipeline assets
- `.github/workflows/platformio-build.yml` builds firmware + LittleFS for both profiles and uploads artifacts
- `./tools/verify_platformio_build.sh` is the local equivalent of that CI build path

### Not claimed by this repository revision
- successful PlatformIO package download/link in every environment
- real hardware validation for your board wiring
- release-grade battery / vibration / UART product closure

---

## 10. Recommended bring-up order

1. `pio run -e esp32s3_n16r8_dev`
2. `pio run -e esp32s3_n16r8_dev -t uploadfs`
3. boot and verify OLED + web asset load
4. verify provisioning AP or STA association
5. verify `/api/meta` shows filesystem + TLS status as expected
6. save Wi-Fi / timezone / location / API token
7. verify weather sync reaches verified TLS state
8. validate one key path and one overlay action
9. then move to `esp32s3_n16r8_release` for sleep/product validation

---

## 11. Known limits

1. Real PlatformIO link/build success still depends on the host having PlatformIO and registry/package access. In this sandbox, `pio` itself can be installed but `espressif32` platform fetch may still fail with `HTTPClientError`, which blocks an honest claim of successful firmware linking here.
2. The CA bundle is embedded in firmware; certificate lifecycle still needs operational maintenance if the provider chain changes.
3. The release profile exists, but true wearable-grade low-power closure still requires real hardware verification.
4. Legacy modules are archived for reference, not supported as active execution paths.

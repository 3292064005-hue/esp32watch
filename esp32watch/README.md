# ESP32-S3 Watch Runtime

**Authoritative runtime for the ESP32-S3 smartwatch platform** — includes device firmware, web control plane, persistent storage management, diagnostics, and release validation tooling.

## ✨ Key Features

- **Staged Runtime Architecture** — optimized main loop with input, sensor, model, UI, and render stages
- **Web Control Plane** — real-time device configuration and monitoring dashboard
- **Persistent Storage** — flash-backed state with durability guarantees and recovery mechanisms
- **Sensor Integration** — MPU6050 IMU with calibration and activity tracking
- **Display & UI** — OLED driver with optimized rendering and power management
- **Network Services** — WiFi provisioning, weather sync, and time synchronization
- **Diagnostic Console** — comprehensive runtime telemetry and error reporting
- **Release Management** — candidate/verified bundle workflow with smoke testing and validation

## 📋 Scope

Core subsystems included in this repository:
- Device runtime with staged main loop architecture
- ESP32-S3 board integration and peripheral drivers (display, input, sensors)
- Web control plane and rich browser console
- Device configuration, storage semantics, state reset flows
- Time/weather sync and network services
- Release packaging, testing, and bundle verification

## 🏗️ Architecture Overview

### Active Source Tree
Primary runtime sources (actively maintained):
- `src/main.cpp` — entry point and initialization
- `src/system_init_esp32.cpp` — hardware and system initialization
- `src/watch_app_runtime*.c` — main application loop and stage orchestration
- `src/services/*` — time, weather, storage, sensors, and system services
- `src/web/*` — web server, API routes, and control plane
- `src/platform_esp32s3.cpp` — ESP32-S3 specific platform layer

### Code Organization
- `src/` — active runtime implementation (C/C++ sources)
- `include/` — public API headers for active build
- `data/` — LittleFS web assets and generated contract files
- `tools/` — build automation, validation, and testing scripts
- `docs/` — detailed architecture and validation documentation
- `partitions/` — ESP32-S3 flash partition definitions
- `.github/workflows/` — CI/CD pipelines for build and deployment
- `src/legacy/` — **archived code** (reference only, excluded from build)

### Build Profiles
Configured in `platformio.ini`:
- **`esp32s3_n16r8_dev`** — development profile with diagnostics and debug logging
- **`esp32s3_n16r8_release`** — production profile with optimizations and validation focus

## 🚀 Getting Started

### Prerequisites
**Minimum (all users):**
- Python 3.8+ — for build scripts and host validation
- POSIX shell environment (bash/zsh) — for build tools

**For firmware compilation:**
- PlatformIO CLI (`pio`) — ESP32 toolchain and build system
- Recommended: PlatformIO Core 6.10.0+ or latest

**For device flashing & testing:**
- ESP32-S3 N16R8 board with USB connection
- Serial monitor software (e.g., `miniterm.py` or PlatformIO terminal)

**For web frontend validation:**
- Node.js 16+ — optional, for smoke testing

### Quick Commands

**Run host-only validation locally (no hardware needed):**
```bash
python3 -m pip install -r tools/requirements-host.txt
./tools/host_sanity_check.sh
```

**Build firmware (dev profile):**
```bash
pio run -e esp32s3_n16r8_dev
```

**Build filesystem image:**
```bash
pio run -e esp32s3_n16r8_dev -t buildfs
```

**Build & upload to device:**
```bash
pio run -e esp32s3_n16r8_dev -t upload
```

**Monitor serial output:**
```bash
pio run -e esp32s3_n16r8_dev -t monitor
```

### Build & Test Paths

#### Path 1: Host-Only Validation (No Hardware Required)
For contract verification, packaging validation, and build checks:
```bash
python3 -m pip install -r tools/requirements-host.txt
./tools/host_sanity_check.sh
```
✅ Validates: contract alignment, serialization, build graph, host behavior  
❌ Does NOT validate: firmware compilation when `pio` is unavailable, GPIO timing, hardware behavior, power-loss scenarios

#### Path 2: Firmware Build (PlatformIO Required)
For local compilation and testing:
```bash
pio run -e esp32s3_n16r8_dev        # Debug build with full logging
pio run -e esp32s3_n16r8_release    # Optimized production build
```
✅ Validates: firmware compilation, resource usage  
❌ Does NOT validate: real hardware behavior until flashed

#### Path 3: Device Validation (Hardware + Lab Access)
For complete device validation and release preparation:
- See `docs/release-validation.md` for detailed procedures
- Includes: candidate bundling, exact flashing, live scenarios, smoke capture, verified promotion

## 📚 Documentation

**Architecture & Design:**
- [`docs/runtime-architecture.md`](docs/runtime-architecture.md) — runtime design, storage semantics, reset flows, boot recovery
- [`docs/release-validation.md`](docs/release-validation.md) — candidate/verified bundle workflow, testing procedures
- [`docs/compatibility-lifecycle.md`](docs/compatibility-lifecycle.md) — compatibility/rollback surface lifecycle and deprecation rules
- [`tools/compatibility_registry.json`](tools/compatibility_registry.json) — machine-readable registry for compatibility surfaces and their required host evidence
- [`docs/state-surface-lifecycle.md`](docs/state-surface-lifecycle.md) — production/consumer/compatibility ownership for state fields
- [`docs/host-simulator-matrix.md`](docs/host-simulator-matrix.md) — host simulator and validation matrix boundaries
- [`docs/control-plane-surfaces.md`](docs/control-plane-surfaces.md) — runtime/control-plane/rich-console surface split

**Runtime Assets (⚠️ Not documentation — runtime contract):**
- `data/contract-bootstrap.json` — bootstrap contract definition
- `data/asset-contract.json` — asset manifest and hashing
  
These files are **part of the runtime contract** and must not be modified without comprehensive validation.

## ❓ Troubleshooting

### Build Issues

**Issue: `pio run` command not found**
```bash
# Option 1: Install PlatformIO globally
pip install platformio

# Option 2: Use Python module invocation
python -m platformio run -e esp32s3_n16r8_dev
```

**Issue: Serial port permission denied (Linux/macOS)**
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in, or run:
newgrp dialout
```

**Issue: LittleFS build fails**
```bash
# Ensure data/ directory exists with web assets
ls -la data/
# Rebuild filesystem
pio run -e esp32s3_n16r8_dev -t buildfs
```

### Runtime Issues

**Device won't connect to WiFi:**
- Check WiFi SSID and password in provisioning mode
- Monitor device logs: `pio run -e esp32s3_n16r8_dev -t monitor`
- Review `docs/runtime-architecture.md` for config authority flow

**Sensor not responding:**
- Verify I2C connections (SDA=GPIO8, SCL=GPIO9)
- Check device logs for initialization errors
- MPU6050 may need recalibration (`sensor_request_calibration()`)

**Web console not accessible:**
- Ensure device is connected to WiFi (check network status in console)
- Access at `http://<device-ip>/` (default port 80)
- Review web routes in `src/web/web_routes_api.cpp`

## ✅ Validation Scope

### What This Repository Proves
- ✅ Contract alignment and consistency
- ✅ Serialization correctness
- ✅ Build graph integrity (no missing dependencies)
- ✅ Local firmware compilation (PlatformIO available)
- ✅ Code organization and static analysis
- ✅ Host-level behavior and logic correctness

### What Requires Real Hardware Testing
- ❌ GPIO timing and sensor accuracy
- ❌ WiFi AP/STA recovery scenarios
- ❌ Power-loss and recovery behavior
- ❌ Real-time performance and latency
- ❌ Peripheral driver interaction with actual hardware

For complete device validation, use the **device-validation path** with real hardware and follow procedures in `docs/release-validation.md`.  Pull requests and ordinary pushes stop at host/build gates; release-grade validation adds flash, scenario, smoke, promotion, and verified-bundle checks. The release workflow consumes a runner capability manifest via `ESP32WATCH_DEVICE_RUNNER_CAPABILITIES_JSON`, and physical lab actions are contract-bound argv arrays rather than free-form shell commands.

## 📊 Validation Status

**Last Updated:** 2026-04-19  
**Host Validation:** Run `./tools/host_sanity_check.sh` for the current branch status  
**Firmware Build Status:** Determined only by PlatformIO build output / CI artifacts  
**Device Verification:** Determined only by verified release bundles and device smoke evidence  
**Current Repository Validation Level:** host/build validation can be established locally; release-grade device validation requires a release workflow run with device evidence and lab attestation

Repository-local files must not be treated as proof of a current successful firmware build unless they are generated from the exact branch revision under validation.

## 📝 Contributing

When modifying this repository:
1. Ensure `pio run -e esp32s3_n16r8_dev` compiles without errors
2. Run host validation: `./tools/host_sanity_check.sh`
3. Update documentation if changing API contracts
4. Do not modify `src/legacy/` without explicit reason
5. Contract assets (`data/*.json`) require validation after changes

## 📄 License

See LICENSE file for licensing information.

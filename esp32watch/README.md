# ESP32-S3 Watch Runtime

This repository contains the **authoritative ESP32-S3 runtime** for the watch project.
It includes the device runtime, Web control plane, persistence/migration path, diagnostics,
and release-validation tooling used for candidate and verified bundles.

## What is in scope
- watch application runtime and staged main loop
- ESP32-S3 board integration and display/input drivers
- Web control plane and rich console
- device configuration, storage semantics, reset flows, and migration status
- time/weather sync services
- release packaging, smoke capture, scenario evidence, and bundle verification

## Active source of truth
Primary active paths:
- `src/main.cpp`
- `src/system_init_esp32.cpp`
- `src/watch_app_runtime*.c`
- `src/services/*`
- `src/web/*`
- `src/platform_esp32s3.cpp`

`src/legacy/` is **archival only**. It is excluded from the authoritative ESP32 build and may be used only as design/reference material during explicit migration work.

## Build profiles
Defined in `platformio.ini`:
- `esp32s3_n16r8_dev` — bring-up, diagnostics, Web-console work
- `esp32s3_n16r8_release` — release-oriented profile with product-style validation focus

## Fast start
### Minimum local prerequisites
- `python3` for packaging, host checks, and verification scripts
- `node` for Web runtime smoke validation
- a POSIX shell environment for `tools/*.sh`

### Optional prerequisites
- `pio` (PlatformIO) for firmware and LittleFS builds
- target board + serial access for flash, live smoke, and scenario validation

### Host-only path (no PlatformIO, no board)
Use this path when you want contract, packaging, and host-behavior coverage only:

```bash
./tools/host_sanity_check.sh
```

This path does **not** prove:
- firmware builds in your local PlatformIO environment
- real hardware GPIO/sensor timing correctness
- real AP/STA recovery on target hardware
- real power-loss behavior on physical boards

### Firmware build path (PlatformIO available)
```bash
pio run -e esp32s3_n16r8_dev
pio run -e esp32s3_n16r8_dev -t buildfs
```

### Device-validation path (board + lab access available)
Follow `docs/release-validation.md` for:
- candidate bundle packaging
- exact candidate flashing
- live scenario execution
- device smoke capture
- verified bundle promotion

## Documentation index
- `docs/runtime-architecture.md` — runtime boundaries, storage/reset semantics, object durability matrix, config authority, boot-loop handling, legacy boundary
- `docs/release-validation.md` — candidate/verified bundle flow, smoke/scenario evidence, local and CI validation sequence

## Runtime assets that are **not** documentation
The following files are runtime contract assets and must not be treated as disposable docs:
- `data/contract-bootstrap.json`
- `data/asset-contract.json`

## Repository layout
- `src/` — active runtime sources
- `include/` — public headers for the active build
- `data/` — LittleFS Web assets and generated contract assets
- `tools/` — build, validation, capture, promotion, and host-test tooling
- `docs/` — maintained human-readable project documentation
- `.github/workflows/` — CI entrypoints for build, capture, promotion, and verification
- `partitions/` — ESP32 partition contract

## Validation scope note
Host/stub validation in this repository proves contract alignment, serialization, build graph integrity,
and selected runtime behavior. It does **not** by itself prove:
- successful `pio run` in every environment
- real hardware GPIO/sensor timing correctness
- real AP/STA recovery on target hardware
- real power-loss behavior on physical boards

Use the release-validation flow for candidate/verified bundles, and treat final device claims as requiring real hardware evidence.

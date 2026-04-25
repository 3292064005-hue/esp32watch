# Runtime Architecture

## 1. Architectural intent
The active ESP32-S3 runtime is designed around a single authoritative path for:
- staged runtime execution
- runtime-owned service coordination
- Web control plane and rich console
- explicit storage durability semantics
- reset/factory-reset flows
- runtime configuration reload
- boot-loop detection and recovery

The repository must not rely on silent dual-track behavior between active code and historical code.

## 2. Active path vs legacy path
### Active path
Authoritative runtime sources live under:
- `src/main.cpp`
- `src/system_init_esp32.cpp`
- `src/watch_app_runtime*.c`
- `src/services/*`
- `src/web/*`
- `src/platform_esp32s3.cpp`

### Legacy boundary
`src/legacy/` is archival only.

Rules:
- Do not add new production dependencies from active code into `src/legacy/`
- Do not treat symbols under `src/legacy/` as runtime-authoritative without explicit migration back into active paths
- Keep migration work explicit in commit history, tests, and release notes

## 3. Runtime main loop
The cooperative runtime stages are owned by one loop and exposed through the stage descriptor manifest:
- INPUT
- SENSOR
- MODEL
- UI
- BATTERY
- ALERT
- DIAG
- STORAGE
- NETWORK
- WEB
- RENDER
- IDLE

This keeps scheduling, telemetry, and fault attribution inside one runtime boundary instead of spreading Web/network work outside the stage budget.

## 4. Storage durability model
Application-visible persistence is expressed through explicit durability levels:
- `VOLATILE`
- `RESET_DOMAIN`
- `DURABLE`

### Current active object matrix
The table below reflects the active semantics exposed by `storage_semantics` and the current ESP32-S3 watch profile.

| Object | Owner | Namespace | Durability | Reset behavior | Survives power loss | Notes |
|---|---|---|---|---|---|---|
| `APP_SETTINGS` | `storage_service` | `app.settings` | `DURABLE` on ESP32-S3 watch when app-state Preferences backend is ready; otherwise `RESET_DOMAIN` | `RESET_APP_STATE` | yes when durable backend is ready | Preferences-backed active path with RTC mirror kept for migration and rollback tolerance |
| `APP_ALARMS` | `storage_service` | `app.alarms` | `DURABLE` on ESP32-S3 watch when app-state Preferences backend is ready; otherwise `RESET_DOMAIN` | `RESET_APP_STATE` | yes when durable backend is ready | Same durability gate and mirror rule as settings |
| `SENSOR_CALIBRATION` | `storage_service` | `sensor.calibration` | `DURABLE` on ESP32-S3 watch when app-state Preferences backend is ready; otherwise `RESET_DOMAIN` | `RESET_APP_STATE` | yes when durable backend is ready | Calibration is cleared during app-state reset/factory reset and re-staged through storage service |
| `GAME_STATS` | `storage_service` | `games.stats` | `DURABLE` | `FACTORY_RESET` | yes | Not cleared by `reset/app-state`; cleared by factory reset |
| `DEVICE_CONFIG` | `device_config` | `device.config` | `DURABLE` | `RESET_DEVICE_CONFIG` | yes | Owns Wi-Fi, timezone, location, and auth-token configuration |
| `TIME_RECOVERY` | `time_service` | `time.recovery` | `DURABLE` | `NEVER_RESET` | yes | Recovery/authority helper state kept across reset domains |

### Runtime rule for app-state durability
For the ESP32-S3 watch profile, `APP_STORAGE_USE_PREFERENCES_APP_STATE=1` enables the Preferences-backed app-state path.
Runtime semantics still evaluate durability dynamically:
- `DURABLE` once the app-state Preferences backend is ready
- `RESET_DOMAIN` if that backend is not ready and the runtime must fall back to the reset-domain mirror path

Durability claims must be made through runtime/storage semantics and validated evidence, not by implication.

## 5. Reset semantics
The runtime distinguishes three reset domains:
- `reset/app-state` — model-owned application state only (`APP_SETTINGS`, `APP_ALARMS`, `SENSOR_CALIBRATION`)
- `reset/device-config` — persisted Wi-Fi/timezone/location/token configuration only (`DEVICE_CONFIG`)
- `reset/factory` — both domains together, including `GAME_STATS`

Reset/factory-reset logic must:
1. stage model/domain changes
2. apply immediate runtime side-effects required for consistency
3. request storage commit
4. complete the synchronous commit path before returning success

## 6. Configuration authority and reload
Device configuration changes are applied through a single authority entry point.

The intended flow is:
1. validate incoming config payload
2. persist through the authoritative config path
3. compute changed domains
4. dispatch runtime reload through the authoritative subscriber path
5. verify that affected services adopted the intended config state
6. expose the reload result through runtime/meta snapshots and API responses

Fallback service-local reconfiguration paths are not the active design target.

The reload/event topology is code-owned by `runtime_reload_event_manifest`; `docs/runtime-reload-event-matrix.md` and `tools/host_tests/test_runtime_reload_event_matrix.py` must stay aligned with that manifest. Hot-apply domains require a live `RuntimeEventSubscription`, a verification hook, and an applied-generation hook. Persisted-only and reboot-required domains must be explicitly represented rather than inferred from missing subscribers.

### Current verification boundary
The active verify path is service-level and authoritative-path aware:
- generation alignment is checked after apply
- Wi-Fi-facing state is verified against the intended config snapshot
- network/time sync state is verified against the intended config snapshot

This is stronger than “event dispatched + snapshot readable”, but it still does not by itself prove every external behavior side effect (for example, successful real-world Wi-Fi association or network reachability).

## 7. Web control plane vs rich console
The Web stack distinguishes:
- **control plane ready** — fallback/API recovery surface is alive
- **console ready** — rich LittleFS-hosted console assets are alive and contract-aligned

This separation prevents filesystem/asset failures from becoming full control-plane failures.

## 8. Boot-loop handling
Crash and boot-loop handling uses:
- crash capsule capture
- consecutive incomplete boot counting
- safe mode entry
- explicit `clearSafeMode` recovery action

This design exists to avoid silent restart-only failure loops and to preserve a minimal recovery/control surface.

## 9. Runtime contract assets
These files are runtime assets, not prose documentation:
- `data/contract-bootstrap.json`
- `data/asset-contract.json`

They are consumed by the backend, frontend, packaging, and verification flows and must stay synchronized with the generated/runtime contract surface.

## 10. Validation boundaries
Repository host validation can prove:
- contract/schema alignment
- build-graph integrity
- packaging/verifier logic
- selected runtime behavior under host stubs

Repository host validation cannot by itself prove:
- target-board GPIO correctness
- physical power-loss behavior
- real Wi-Fi/STA/AP behavior
- real TLS/network timing

Those claims require real device evidence.


## Startup ordering note

Authority services start before consumer services on the active boot path:
1. storage/config authority
2. time recovery baseline
3. network/time consumer services

The active plan is now explicit in code via `system_runtime_init_profile()`, `system_runtime_service_plan_stage()`, and prerequisite assertions checked by `system_runtime_service_init()`. The default profile is `STORAGE_AUTHORITY_FIRST`; rollback remains available through the `SYSTEM_RUNTIME_INIT_STORAGE_AUTHORITY_FIRST=0` build override.

This keeps persisted configuration and durability truth available before runtime consumers attempt reload or sync behavior while preserving a bounded rollback switch for staged rollout.


- Compatibility aggregate reads are synthesized on demand; retired write-side legacy mutation/sync entry points are no longer part of the supported internal surface.
- Runtime-service rollback remains available through `SYSTEM_RUNTIME_INIT_STORAGE_AUTHORITY_FIRST=0`, and host validation exercises both startup profiles.


## 11. RTC init-stage semantics
`SYSTEM_INIT_STAGE_RTC` now means **RTC reset-domain / backup-domain readiness**, not proof that the platform exposes a persistent wall-clock authority. Runtime callers must interpret wall-clock capability through `platformSupport.rtcWallClock` and `platformSupport.rtcWallClockPersistent`, not through the init-stage label alone.

For the current ESP32-S3 profile:
- `SYSTEM_INIT_STAGE_RTC` / `RTC_DOMAIN` can complete successfully
- `platformSupport.rtcResetDomain` is true
- `platformSupport.rtcWallClock` is false
- `platformSupport.rtcWallClockPersistent` is false

## 12. Time authority model
The active time authority ladder is:
- `HARDWARE` — platform RTC provides a reasonable wall-clock epoch
- `NETWORK` — NTP / companion / network sync has established a trustworthy epoch
- `HOST` — host-provided time in controlled test flows
- `RECOVERY` — persisted recovery epoch only
- `NONE` — no usable epoch baseline

State, meta, and health outputs must describe this authority explicitly through `timeAuthority`, and startup/init diagnostics must not imply wall-clock truth solely because `RTC_DOMAIN` completed.

## 13. Companion protocol contract

The companion protocol exposes its advertised version, capabilities, GET subject catalog, and EXPORT subject catalog through `companion_proto_contract.*`. Parser and handler code must not advertise a command or subject that is not implemented by the active protocol surface. The advertised capability list intentionally includes only implemented command capabilities such as `safeclr` and `sensorreinit`; unsupported experimental commands must stay absent until parser and handler support are implemented together.

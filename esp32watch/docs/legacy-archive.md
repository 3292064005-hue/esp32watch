# Legacy / Bluepill Archive Boundary

This repository keeps the historical Bluepill and legacy sources only as reference material.

## Active runtime target

The active target is the ESP32-S3 watch profile selected by `APP_BOARD_PROFILE_ESP32S3_WATCH` in `platformio.ini`.

## Archived surfaces

The following surfaces are archived and are not part of the active ESP32-S3 build:

- `src/legacy/`
- STM32 startup/HAL compatibility files excluded by `build_src_filter`
- Bluepill board profiles in `include/board_features.h`

## Change rule

Do not modify archived surfaces while landing ESP32-S3 runtime or web-control changes unless the change explicitly declares a legacy migration. Active-path fixes must be made in the ESP32-S3 runtime files, not by reviving legacy fallback code.

## Migration rule

If a legacy implementation is intentionally restored, add a dedicated migration note that covers:

1. active build target impact,
2. compile gate or profile gate,
3. runtime entry point,
4. verification command,
5. rollback path.


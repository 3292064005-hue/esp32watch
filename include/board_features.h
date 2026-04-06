#ifndef BOARD_FEATURES_H
#define BOARD_FEATURES_H

/*
 * Hardware profile selection.
 * Keep exactly one APP_BOARD_PROFILE_* enabled at build time.
 */
#if !defined(APP_BOARD_PROFILE_BLUEPILL_CORE) && \
    !defined(APP_BOARD_PROFILE_BLUEPILL_BATTERY) && \
    !defined(APP_BOARD_PROFILE_BLUEPILL_FULL) && \
    !defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
#define APP_BOARD_PROFILE_ESP32S3_WATCH 1
#endif

#if defined(APP_BOARD_PROFILE_BLUEPILL_FULL)
#define APP_FEATURE_BATTERY             1
#define APP_FEATURE_VIBRATION           1
#define APP_FEATURE_SENSOR              1
#define APP_STORAGE_USE_FLASH           1
#define APP_STORAGE_MIRROR_BKP_WHEN_FLASH 1
#define APP_FEATURE_WATCHDOG            1
#define APP_FEATURE_COMPANION_UART      1
#elif defined(APP_BOARD_PROFILE_BLUEPILL_BATTERY)
#define APP_FEATURE_BATTERY             1
#define APP_FEATURE_VIBRATION           0
#define APP_FEATURE_SENSOR              1
#define APP_STORAGE_USE_FLASH           0
#define APP_STORAGE_MIRROR_BKP_WHEN_FLASH 0
#define APP_FEATURE_WATCHDOG            0
#define APP_FEATURE_COMPANION_UART      1
#elif defined(APP_BOARD_PROFILE_ESP32S3_WATCH)
#define APP_FEATURE_BATTERY             0
#define APP_FEATURE_VIBRATION           0
#define APP_FEATURE_SENSOR              1
#define APP_STORAGE_USE_FLASH           0
#define APP_STORAGE_MIRROR_BKP_WHEN_FLASH 0
#define APP_FEATURE_WATCHDOG            0
#define APP_FEATURE_COMPANION_UART      0
#else
#define APP_FEATURE_BATTERY             0
#define APP_FEATURE_VIBRATION           0
#define APP_FEATURE_SENSOR              1
#define APP_STORAGE_USE_FLASH           0
#define APP_STORAGE_MIRROR_BKP_WHEN_FLASH 0
#define APP_FEATURE_WATCHDOG            0
#define APP_FEATURE_COMPANION_UART      1
#endif

#endif

#ifndef APP_DEFAULTS_H
#define APP_DEFAULTS_H

#define SCREEN_SLEEP_DEFAULT_MS         15000U
#define SCREEN_SLEEP_LONG_MS            30000U
#define SCREEN_SLEEP_MAX_MS             60000U

#define DEFAULT_STEP_GOAL               8000U
#define DEFAULT_TIMER_SECONDS           300U
#define DEFAULT_BRIGHTNESS              2U
#define DEFAULT_SENSOR_SENSITIVITY      1U
#define DEFAULT_WATCHFACE               0U
#define DEFAULT_ALARM_HOUR              7U
#define DEFAULT_ALARM_MINUTE            30U
#define DEFAULT_SHOW_SECONDS            1U
#define DEFAULT_ANIMATIONS              1U
#define DEFAULT_AUTO_WAKE               1U
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
#define DEFAULT_AUTO_SLEEP              0U
#else
#define DEFAULT_AUTO_SLEEP              1U
#endif
#define DEFAULT_DND                     0U

#endif

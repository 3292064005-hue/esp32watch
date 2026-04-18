#ifndef PERSIST_PREFERENCES_H
#define PERSIST_PREFERENCES_H

#include <Preferences.h>

extern "C" {
#include "persist_namespace_config.h"
}

enum PersistPreferencesDomain {
    PERSIST_PREFS_DOMAIN_DEVICE_CONFIG = 0,
    PERSIST_PREFS_DOMAIN_TIME_RECOVERY,
    PERSIST_PREFS_DOMAIN_GAME_STATS,
    PERSIST_PREFS_DOMAIN_APP_SETTINGS,
    PERSIST_PREFS_DOMAIN_APP_ALARMS,
    PERSIST_PREFS_DOMAIN_SENSOR_CALIBRATION,
};

/**
 * @brief Resolve the durable Preferences namespace for a storage domain.
 *
 * @param[in] domain Durable preferences ownership domain.
 * @return Namespace string for the requested domain, or nullptr when @p domain is invalid.
 * @throws None.
 */
const char *persist_preferences_namespace(PersistPreferencesDomain domain);

/**
 * @brief Open a Preferences handle for a specific durable storage domain.
 *
 * @param[in,out] prefs Preferences instance to open.
 * @param[in] domain Durable preferences ownership domain.
 * @param[in] read_only True to open read-only; false for read-write access.
 * @return true when the namespace was resolved and opened; false on invalid input or backend failure.
 * @throws None.
 * @boundary_behavior Does not close an already-open Preferences handle; callers remain responsible for `end()`.
 */
bool persist_preferences_begin(Preferences &prefs, PersistPreferencesDomain domain, bool read_only);

#endif

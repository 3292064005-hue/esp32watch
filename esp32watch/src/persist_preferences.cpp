#include "persist_preferences.h"

const char *persist_preferences_namespace(PersistPreferencesDomain domain)
{
    switch (domain) {
        case PERSIST_PREFS_DOMAIN_DEVICE_CONFIG:
            return APP_PREFS_NS_DEVICE_CONFIG;
        case PERSIST_PREFS_DOMAIN_TIME_RECOVERY:
            return APP_PREFS_NS_TIME_RECOVERY;
        case PERSIST_PREFS_DOMAIN_GAME_STATS:
            return APP_PREFS_NS_GAME_STATS;
        case PERSIST_PREFS_DOMAIN_APP_SETTINGS:
            return APP_PREFS_NS_APP_SETTINGS;
        case PERSIST_PREFS_DOMAIN_APP_ALARMS:
            return APP_PREFS_NS_APP_ALARMS;
        case PERSIST_PREFS_DOMAIN_SENSOR_CALIBRATION:
            return APP_PREFS_NS_SENSOR_CALIBRATION;
        default:
            return nullptr;
    }
}

bool persist_preferences_begin(Preferences &prefs, PersistPreferencesDomain domain, bool read_only)
{
    const char *ns = persist_preferences_namespace(domain);
    if (ns == nullptr) {
        return false;
    }
    return prefs.begin(ns, read_only);
}

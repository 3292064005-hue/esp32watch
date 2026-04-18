#include "services/device_config_authority.h"
#include "services/storage_facade.h"
#include <string.h>

static bool float_equal(float lhs, float rhs)
{
    return lhs == rhs;
}

static void apply_report_init(DeviceConfigAuthorityApplyReport *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
}

static bool config_strings_equal(const char *lhs, const char *rhs)
{
    if (lhs == NULL || rhs == NULL) {
        return lhs == rhs;
    }
    return strcmp(lhs, rhs) == 0;
}

static bool authority_load_current(DeviceConfigSnapshot *cfg,
                                   char *wifi_password,
                                   uint32_t wifi_password_size,
                                   char *api_token,
                                   uint32_t api_token_size)
{
    if (cfg == NULL || wifi_password == NULL || api_token == NULL) {
        return false;
    }
    if (!storage_facade_get_device_config(cfg)) {
        return false;
    }
    if (!storage_facade_get_device_wifi_password(wifi_password, wifi_password_size)) {
        return false;
    }
    if (!storage_facade_get_device_api_token(api_token, api_token_size)) {
        return false;
    }
    return true;
}

static void authority_compute_diff(const DeviceConfigUpdate *update,
                                   const DeviceConfigSnapshot *current_cfg,
                                   const char *current_wifi_password,
                                   const char *current_api_token,
                                   DeviceConfigAuthorityApplyReport *out)
{
    if (update == NULL || current_cfg == NULL || current_wifi_password == NULL || current_api_token == NULL || out == NULL) {
        return;
    }

    out->wifi_saved = update->set_wifi;
    out->network_saved = update->set_network;
    out->token_saved = update->set_api_token;

    if (update->set_wifi) {
        out->wifi_changed = !config_strings_equal(update->wifi_ssid, current_cfg->wifi_ssid) ||
                            !config_strings_equal(update->wifi_password, current_wifi_password);
    }
    if (update->set_network) {
        out->network_changed = !config_strings_equal(update->timezone_posix, current_cfg->timezone_posix) ||
                               !config_strings_equal(update->timezone_id, current_cfg->timezone_id) ||
                               !config_strings_equal(update->location_name, current_cfg->location_name) ||
                               !float_equal(update->latitude, current_cfg->latitude) ||
                               !float_equal(update->longitude, current_cfg->longitude);
    }
    if (update->set_api_token) {
        out->token_changed = !config_strings_equal(update->api_token, current_api_token);
    }
}

bool device_config_authority_get(DeviceConfigSnapshot *out)
{
    return storage_facade_get_device_config(out);
}

bool device_config_authority_has_token(void)
{
    return storage_facade_device_config_has_api_token();
}

bool device_config_authority_authenticate_token(const char *token)
{
    return storage_facade_authenticate_device_token(token);
}

bool device_config_authority_apply_update(const DeviceConfigUpdate *update,
                                          DeviceConfigAuthorityApplyReport *out)
{
    DeviceConfigSnapshot current_cfg = {0};
    DeviceConfigAuthorityApplyReport local_report = {0};
    DeviceConfigAuthorityApplyReport *report = out != NULL ? out : &local_report;
    char current_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
    char current_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {0};

    apply_report_init(report);
    if (update == NULL) {
        return false;
    }
    if (!authority_load_current(&current_cfg,
                                current_wifi_password,
                                sizeof(current_wifi_password),
                                current_api_token,
                                sizeof(current_api_token))) {
        return false;
    }

    authority_compute_diff(update, &current_cfg, current_wifi_password, current_api_token, report);

    if (!storage_facade_apply_device_config_update(update)) {
        return false;
    }

    {
        const bool runtime_reload_requested = report->wifi_changed || report->network_changed;
        report->committed = true;
        report->runtime_reload_requested = runtime_reload_requested;
        if (!runtime_reload_requested) {
            report->runtime_reload_ok = true;
            return true;
        }
        report->runtime_reload_ok = runtime_reload_device_config(report->wifi_changed,
                                                                 report->network_changed,
                                                                 &report->reload);
        return report->runtime_reload_ok;
    }
}

bool device_config_authority_restore_defaults(DeviceConfigAuthorityApplyReport *out)
{
    apply_report_init(out);
    if (!storage_facade_restore_device_config_defaults()) {
        return false;
    }
    if (out != NULL) {
        out->committed = true;
        out->wifi_saved = true;
        out->network_saved = true;
        out->token_saved = true;
        out->wifi_changed = true;
        out->network_changed = true;
        out->token_changed = true;
        out->runtime_reload_requested = true;
        out->runtime_reload_ok = runtime_reload_device_config(true, true, &out->reload);
        return out->runtime_reload_ok;
    }
    return runtime_reload_device_config(true, true, NULL);
}

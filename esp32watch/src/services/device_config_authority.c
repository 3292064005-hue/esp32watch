#include "services/device_config_authority.h"
#include "services/device_config_codec.h"
#include "services/storage_facade.h"
#include "services/storage_facade_device_config_backend.h"
#include <string.h>

static void apply_report_init(DeviceConfigAuthorityApplyReport *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
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


static uint32_t authority_expand_runtime_reload_affinity(uint32_t changed_domain_mask)
{
    uint32_t expanded = changed_domain_mask;

    if ((changed_domain_mask & RUNTIME_RELOAD_DOMAIN_WIFI) != 0U) {
        expanded |= RUNTIME_RELOAD_DOMAIN_POWER;
        expanded |= RUNTIME_RELOAD_DOMAIN_COMPANION;
    }
    if ((changed_domain_mask & RUNTIME_RELOAD_DOMAIN_NETWORK) != 0U) {
        expanded |= RUNTIME_RELOAD_DOMAIN_DISPLAY;
        expanded |= RUNTIME_RELOAD_DOMAIN_POWER;
        expanded |= RUNTIME_RELOAD_DOMAIN_SENSOR;
        expanded |= RUNTIME_RELOAD_DOMAIN_COMPANION;
    }
    if ((changed_domain_mask & RUNTIME_RELOAD_DOMAIN_AUTH) != 0U) {
        expanded |= RUNTIME_RELOAD_DOMAIN_COMPANION;
    }

    return expanded;
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
        out->saved_domain_mask |= RUNTIME_RELOAD_DOMAIN_WIFI;
        out->wifi_changed = strcmp(update->wifi_ssid, current_cfg->wifi_ssid) != 0 ||
                            strcmp(update->wifi_password, current_wifi_password) != 0;
        if (out->wifi_changed) {
            out->changed_domain_mask |= RUNTIME_RELOAD_DOMAIN_WIFI;
        }
    }
    if (update->set_network) {
        out->saved_domain_mask |= RUNTIME_RELOAD_DOMAIN_NETWORK;
        out->network_changed = !device_config_text_equal_canonical(update->timezone_posix, current_cfg->timezone_posix) ||
                               !device_config_text_equal_canonical(update->timezone_id, current_cfg->timezone_id) ||
                               !device_config_text_equal_canonical(update->location_name, current_cfg->location_name) ||
                               !device_config_coordinate_equal(update->latitude, current_cfg->latitude) ||
                               !device_config_coordinate_equal(update->longitude, current_cfg->longitude);
        if (out->network_changed) {
            out->changed_domain_mask |= RUNTIME_RELOAD_DOMAIN_NETWORK;
        }
    }
    if (update->set_api_token) {
        out->saved_domain_mask |= RUNTIME_RELOAD_DOMAIN_AUTH;
        out->token_changed = strcmp(update->api_token, current_api_token) != 0;
        if (out->token_changed) {
            out->changed_domain_mask |= RUNTIME_RELOAD_DOMAIN_AUTH;
        }
    }

    if (update->request_runtime_reload) {
        out->changed_domain_mask |= update->runtime_reload_domain_mask;
    }

    out->changed_domain_mask = authority_expand_runtime_reload_affinity(out->changed_domain_mask);
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
    DeviceConfigUpdate persist_update = {0};
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

    persist_update = *update;
    device_config_canonicalize_update(&persist_update);
    authority_compute_diff(&persist_update, &current_cfg, current_wifi_password, current_api_token, report);

    if (!storage_facade_device_config_backend_apply_update(&persist_update)) {
        return false;
    }

    report->committed = true;
    report->runtime_reload_requested_domain_mask = report->changed_domain_mask & runtime_reload_supported_domain_mask();
    report->runtime_reload_requested = report->runtime_reload_requested_domain_mask != RUNTIME_RELOAD_DOMAIN_NONE;
    if (!report->runtime_reload_requested) {
        report->runtime_reload_ok = true;
        return true;
    }
    report->runtime_reload_ok = runtime_reload_device_config_domains(report->runtime_reload_requested_domain_mask,
                                                                     &report->reload);
    return report->runtime_reload_ok;
}

bool device_config_authority_restore_defaults(DeviceConfigAuthorityApplyReport *out)
{
    DeviceConfigAuthorityApplyReport local_report = {0};
    DeviceConfigAuthorityApplyReport *report = out != NULL ? out : &local_report;

    apply_report_init(report);
    if (!storage_facade_device_config_backend_restore_defaults()) {
        return false;
    }
    report->committed = true;
    report->wifi_saved = true;
    report->network_saved = true;
    report->token_saved = true;
    report->wifi_changed = true;
    report->network_changed = true;
    report->token_changed = true;
    report->saved_domain_mask = RUNTIME_RELOAD_DOMAIN_WIFI | RUNTIME_RELOAD_DOMAIN_NETWORK | RUNTIME_RELOAD_DOMAIN_AUTH;
    report->changed_domain_mask = authority_expand_runtime_reload_affinity(report->saved_domain_mask);
    report->runtime_reload_requested_domain_mask = report->changed_domain_mask & runtime_reload_supported_domain_mask();
    report->runtime_reload_requested = true;
    report->runtime_reload_ok = runtime_reload_device_config_domains(report->runtime_reload_requested_domain_mask,
                                                                     &report->reload);
    return report->runtime_reload_ok;
}

#ifndef SERVICES_DEVICE_CONFIG_AUTHORITY_H
#define SERVICES_DEVICE_CONFIG_AUTHORITY_H

#include <stdbool.h>
#include <stdint.h>
#include "services/device_config.h"
#include "services/runtime_reload_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool committed;
    bool wifi_saved;
    bool network_saved;
    bool token_saved;
    bool wifi_changed;
    bool network_changed;
    bool token_changed;
    bool runtime_reload_requested;
    bool runtime_reload_ok;
    uint32_t saved_domain_mask;
    uint32_t changed_domain_mask;
    uint32_t runtime_reload_requested_domain_mask;
    RuntimeReloadReport reload;
} DeviceConfigAuthorityApplyReport;

bool device_config_authority_get(DeviceConfigSnapshot *out);
bool device_config_authority_has_token(void);
bool device_config_authority_authenticate_token(const char *token);
bool device_config_authority_apply_update(const DeviceConfigUpdate *update,
                                          DeviceConfigAuthorityApplyReport *out);
bool device_config_authority_restore_defaults(DeviceConfigAuthorityApplyReport *out);

#ifdef __cplusplus
}
#endif

#endif

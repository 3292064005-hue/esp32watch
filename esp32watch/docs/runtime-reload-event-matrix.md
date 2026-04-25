# Runtime Reload / Event Matrix

This document mirrors the canonical manifest in `include/services/runtime_reload_event_manifest.h` and `src/services/runtime_reload_event_manifest.c`. The manifest is the code-owned topology source; this document is the human-readable review surface.

## Device-config reload domains

| Domain constant | Domain | Primary consumer | Subscription event | Apply strategy | Verification hook | Strong business consumer | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `RUNTIME_RELOAD_DOMAIN_WIFI` | WIFI | `web_wifi` | `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `HOT_APPLY` | `web_wifi_verify_config_applied` | yes | `src/web/web_wifi.cpp` |
| `RUNTIME_RELOAD_DOMAIN_NETWORK` | NETWORK | `network_sync_service` | `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `HOT_APPLY` | `network_sync_service_verify_config_applied` | yes | `src/services/network_sync_service.cpp` |
| `RUNTIME_RELOAD_DOMAIN_AUTH` | AUTH | `device_config_authority` | `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `PERSISTED_ONLY` | `runtime_reload_passthrough_verify` | no, persisted config authority only | `src/services/device_config_authority.c` |
| `RUNTIME_RELOAD_DOMAIN_DISPLAY` | DISPLAY | `display_service` | `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `HOT_APPLY` | `display_service_verify_config_applied` | yes | `src/services/display_service.c` |
| `RUNTIME_RELOAD_DOMAIN_POWER` | POWER | `power_service` | `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `HOT_APPLY` | `power_service_verify_config_applied` | yes | `src/services/power_service.c` |
| `RUNTIME_RELOAD_DOMAIN_SENSOR` | SENSOR | `sensor_service` | `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `HOT_APPLY` | `sensor_service_verify_config_applied` | yes | `src/services/sensor_service.c` |
| `RUNTIME_RELOAD_DOMAIN_COMPANION` | COMPANION | `companion_runtime` | `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `REQUIRES_REBOOT` | `runtime_reload_passthrough_verify` | no, reboot-bound runtime surface | `src/companion_transport.c` |

## Runtime event producers

| Event | Producer | Consumer class | Strong business consumer required | Evidence |
| --- | --- | --- | --- | --- |
| `RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED` | `device_config_authority` | hot-apply reload consumers | yes | `src/services/device_config_authority.c`, `src/services/runtime_reload_coordinator.c` |
| `RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED` | `power_service` | diagnostics / observers | no | `src/services/power_service.c` |
| `RUNTIME_SERVICE_EVENT_STORAGE_COMMIT_FINISHED` | `storage_service` | diagnostics / observers | no | `src/services/storage_service.c` |
| `RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED` | `reset_service` | audit / diagnostics | no | `src/services/reset_service.c` |
| `RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED` | `reset_service` | audit / diagnostics | no | `src/services/reset_service.c` |
| `RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED` | `reset_service` | audit / diagnostics | no | `src/services/reset_service.c` |

## Validation rule

`tools/host_tests/test_runtime_reload_event_matrix.py` enforces the topology from the manifest, not from this prose alone. It checks that:

1. every manifest reload domain is declared by the coordinator and documented above;
2. every hot-apply domain has a live subscription, domain mask, verification hook, and applied-generation hook in its consumer source;
3. persisted-only and reboot-required domains are explicitly documented as non-hot-apply domains;
4. every manifest event producer is backed by a publish call or coordinator dispatch path in live code;
5. the document remains synchronized with the manifest.

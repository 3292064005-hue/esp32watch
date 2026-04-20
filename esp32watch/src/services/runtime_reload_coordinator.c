#include "services/runtime_reload_coordinator.h"

#include "services/device_config.h"
#include "services/display_service.h"
#include "services/network_sync_service.h"
#include "services/power_service.h"
#include "services/runtime_event_service.h"
#include "services/sensor_service.h"
#include "web/web_wifi.h"
#include <string.h>

typedef bool (*RuntimeReloadVerifyFn)(uint32_t generation, const DeviceConfigSnapshot *cfg);
typedef uint32_t (*RuntimeReloadGenerationFn)(void);

typedef struct {
    uint32_t domain_mask;
    const char *domain_name;
    RuntimeReloadApplyStrategy strategy;
    RuntimeReloadVerifyFn verify;
    RuntimeReloadGenerationFn applied_generation;
    bool *legacy_ok;
    uint32_t *legacy_generation;
    const char **legacy_reason;
    const char *verify_reason_ok;
    const char *verify_reason_fail;
    const char *verify_failure_code;
} RuntimeReloadDomainSpec;

static const char *kVerifyReasonNotRequested = "NONE";
static const char *kVerifyReasonNoHandler = "NO_SUBSCRIBER";
static const char *kVerifyReasonDispatchFailed = "EVENT_DISPATCH_FAILED";
static const char *kVerifyReasonPersistedOnly = "PERSISTED_ONLY";
static const char *kVerifyReasonRequiresReboot = "REQUIRES_REBOOT";

static bool runtime_reload_preflight_mask(uint32_t requested_domain_mask, RuntimeReloadReport *report);
static bool runtime_reload_verify_mask(uint32_t requested_domain_mask, RuntimeReloadReport *report);
static bool runtime_reload_preflight(bool wifi_changed,
                                     bool network_changed,
                                     RuntimeReloadReport *report);
static bool runtime_reload_verify(bool wifi_changed,
                                  bool network_changed,
                                  RuntimeReloadReport *report);

static uint32_t runtime_reload_passthrough_generation(void)
{
    return device_config_generation();
}

static bool runtime_reload_passthrough_verify(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    return generation != 0U && cfg != NULL;
}

const char *runtime_reload_domain_name(uint32_t domain_mask)
{
    switch (domain_mask) {
        case RUNTIME_RELOAD_DOMAIN_WIFI: return "WIFI";
        case RUNTIME_RELOAD_DOMAIN_NETWORK: return "NETWORK";
        case RUNTIME_RELOAD_DOMAIN_AUTH: return "AUTH";
        case RUNTIME_RELOAD_DOMAIN_DISPLAY: return "DISPLAY";
        case RUNTIME_RELOAD_DOMAIN_POWER: return "POWER";
        case RUNTIME_RELOAD_DOMAIN_SENSOR: return "SENSOR";
        case RUNTIME_RELOAD_DOMAIN_COMPANION: return "COMPANION";
        default: return "UNKNOWN";
    }
}

const char *runtime_reload_apply_strategy_name(RuntimeReloadApplyStrategy strategy)
{
    switch (strategy) {
        case RUNTIME_RELOAD_APPLY_HOT: return "HOT_APPLY";
        case RUNTIME_RELOAD_APPLY_PERSISTED_ONLY: return "PERSISTED_ONLY";
        case RUNTIME_RELOAD_APPLY_REQUIRES_REBOOT: return "REQUIRES_REBOOT";
        default: return "UNKNOWN";
    }
}

static const RuntimeReloadDomainSpec *runtime_reload_domain_specs(RuntimeReloadReport *report, size_t *count)
{
    static RuntimeReloadDomainSpec specs[7];

    specs[0].domain_mask = RUNTIME_RELOAD_DOMAIN_WIFI;
    specs[0].domain_name = "WIFI";
    specs[0].strategy = RUNTIME_RELOAD_APPLY_HOT;
    specs[0].verify = web_wifi_verify_config_applied;
    specs[0].applied_generation = web_wifi_last_applied_generation;
    specs[0].legacy_ok = report != NULL ? &report->wifi_reload_ok : NULL;
    specs[0].legacy_generation = report != NULL ? &report->wifi_applied_generation : NULL;
    specs[0].legacy_reason = report != NULL ? &report->wifi_verify_reason : NULL;
    specs[0].verify_reason_ok = "CONFIG_APPLIED";
    specs[0].verify_reason_fail = "CONFIG_MISMATCH";
    specs[0].verify_failure_code = "VERIFY_WIFI_CONFIG_MISMATCH";

    specs[1].domain_mask = RUNTIME_RELOAD_DOMAIN_NETWORK;
    specs[1].domain_name = "NETWORK";
    specs[1].strategy = RUNTIME_RELOAD_APPLY_HOT;
    specs[1].verify = network_sync_service_verify_config_applied;
    specs[1].applied_generation = network_sync_service_last_applied_generation;
    specs[1].legacy_ok = report != NULL ? &report->network_reload_ok : NULL;
    specs[1].legacy_generation = report != NULL ? &report->network_applied_generation : NULL;
    specs[1].legacy_reason = report != NULL ? &report->network_verify_reason : NULL;
    specs[1].verify_reason_ok = "CONFIG_APPLIED";
    specs[1].verify_reason_fail = "CONFIG_MISMATCH";
    specs[1].verify_failure_code = "VERIFY_NETWORK_CONFIG_MISMATCH";

    specs[2].domain_mask = RUNTIME_RELOAD_DOMAIN_AUTH;
    specs[2].domain_name = "AUTH";
    specs[2].strategy = RUNTIME_RELOAD_APPLY_PERSISTED_ONLY;
    specs[2].verify = runtime_reload_passthrough_verify;
    specs[2].applied_generation = runtime_reload_passthrough_generation;
    specs[2].legacy_ok = NULL;
    specs[2].legacy_generation = NULL;
    specs[2].legacy_reason = NULL;
    specs[2].verify_reason_ok = kVerifyReasonPersistedOnly;
    specs[2].verify_reason_fail = "CONFIG_UNAVAILABLE";
    specs[2].verify_failure_code = "VERIFY_AUTH_CONFIG_UNAVAILABLE";

    specs[3].domain_mask = RUNTIME_RELOAD_DOMAIN_DISPLAY;
    specs[3].domain_name = "DISPLAY";
    specs[3].strategy = RUNTIME_RELOAD_APPLY_HOT;
    specs[3].verify = display_service_verify_config_applied;
    specs[3].applied_generation = display_service_last_applied_generation;
    specs[3].legacy_ok = NULL;
    specs[3].legacy_generation = NULL;
    specs[3].legacy_reason = NULL;
    specs[3].verify_reason_ok = "CONFIG_APPLIED";
    specs[3].verify_reason_fail = "CONFIG_MISMATCH";
    specs[3].verify_failure_code = "VERIFY_DISPLAY_CONFIG_MISMATCH";

    specs[4].domain_mask = RUNTIME_RELOAD_DOMAIN_POWER;
    specs[4].domain_name = "POWER";
    specs[4].strategy = RUNTIME_RELOAD_APPLY_HOT;
    specs[4].verify = power_service_verify_config_applied;
    specs[4].applied_generation = power_service_last_applied_generation;
    specs[4].legacy_ok = NULL;
    specs[4].legacy_generation = NULL;
    specs[4].legacy_reason = NULL;
    specs[4].verify_reason_ok = "CONFIG_APPLIED";
    specs[4].verify_reason_fail = "CONFIG_MISMATCH";
    specs[4].verify_failure_code = "VERIFY_POWER_CONFIG_MISMATCH";

    specs[5].domain_mask = RUNTIME_RELOAD_DOMAIN_SENSOR;
    specs[5].domain_name = "SENSOR";
    specs[5].strategy = RUNTIME_RELOAD_APPLY_HOT;
    specs[5].verify = sensor_service_verify_config_applied;
    specs[5].applied_generation = sensor_service_last_applied_generation;
    specs[5].legacy_ok = NULL;
    specs[5].legacy_generation = NULL;
    specs[5].legacy_reason = NULL;
    specs[5].verify_reason_ok = "CONFIG_APPLIED";
    specs[5].verify_reason_fail = "CONFIG_MISMATCH";
    specs[5].verify_failure_code = "VERIFY_SENSOR_CONFIG_MISMATCH";

    specs[6].domain_mask = RUNTIME_RELOAD_DOMAIN_COMPANION;
    specs[6].domain_name = "COMPANION";
    specs[6].strategy = RUNTIME_RELOAD_APPLY_REQUIRES_REBOOT;
    specs[6].verify = runtime_reload_passthrough_verify;
    specs[6].applied_generation = runtime_reload_passthrough_generation;
    specs[6].legacy_ok = NULL;
    specs[6].legacy_generation = NULL;
    specs[6].legacy_reason = NULL;
    specs[6].verify_reason_ok = kVerifyReasonRequiresReboot;
    specs[6].verify_reason_fail = "CONFIG_UNAVAILABLE";
    specs[6].verify_failure_code = "VERIFY_COMPANION_CONFIG_UNAVAILABLE";

    if (count != NULL) {
        *count = sizeof(specs) / sizeof(specs[0]);
    }
    return specs;
}

static const RuntimeReloadDomainSpec *runtime_reload_find_domain_spec(uint32_t domain_mask,
                                                                      RuntimeReloadReport *report)
{
    size_t count = 0U;
    const RuntimeReloadDomainSpec *specs = runtime_reload_domain_specs(report, &count);
    size_t i;

    for (i = 0U; i < count; ++i) {
        if (specs[i].domain_mask == domain_mask) {
            return &specs[i];
        }
    }
    return NULL;
}

static uint32_t runtime_reload_supported_domain_mask_from_specs(void)
{
    size_t count = 0U;
    const RuntimeReloadDomainSpec *specs = runtime_reload_domain_specs(NULL, &count);
    uint32_t mask = RUNTIME_RELOAD_DOMAIN_NONE;
    size_t i;

    for (i = 0U; i < count; ++i) {
        mask |= specs[i].domain_mask;
    }
    return mask;
}

static uint32_t runtime_reload_domain_mask_for_strategy(RuntimeReloadApplyStrategy strategy)
{
    size_t count = 0U;
    const RuntimeReloadDomainSpec *specs = runtime_reload_domain_specs(NULL, &count);
    uint32_t mask = RUNTIME_RELOAD_DOMAIN_NONE;
    size_t i;

    for (i = 0U; i < count; ++i) {
        if (specs[i].strategy == strategy) {
            mask |= specs[i].domain_mask;
        }
    }
    return mask;
}

uint32_t runtime_reload_supported_domain_mask(void)
{
    return runtime_reload_supported_domain_mask_from_specs();
}

static uint32_t runtime_reload_requested_from_legacy(bool wifi_changed, bool network_changed)
{
    uint32_t requested_domain_mask = RUNTIME_RELOAD_DOMAIN_NONE;
    if (wifi_changed) {
        requested_domain_mask |= RUNTIME_RELOAD_DOMAIN_WIFI;
    }
    if (network_changed) {
        requested_domain_mask |= RUNTIME_RELOAD_DOMAIN_NETWORK;
    }
    return requested_domain_mask;
}

static void runtime_reload_set_failure(RuntimeReloadReport *report,
                                       const char *phase,
                                       const char *code,
                                       uint32_t failed_domain_mask)
{
    if (report == NULL) {
        return;
    }
    report->failure_phase = phase != NULL ? phase : "NONE";
    report->failure_code = code != NULL ? code : "NONE";
    report->failed_domain_mask |= failed_domain_mask;
}

static void runtime_reload_report_init(RuntimeReloadReport *out, bool requested)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->requested = requested;
    out->preflight_ok = !requested;
    out->wifi_reload_ok = true;
    out->network_reload_ok = true;
    out->verify_ok = !requested;
    out->first_failed_handler_name = "NONE";
    out->failure_phase = "NONE";
    out->failure_code = "NONE";
    out->wifi_verify_reason = kVerifyReasonNotRequested;
    out->network_verify_reason = kVerifyReasonNotRequested;
}

static void runtime_reload_report_copy(RuntimeReloadReport *dst, const RuntimeReloadReport *src)
{
    if (dst == NULL || src == NULL) {
        return;
    }
    *dst = *src;
}

static RuntimeReloadDomainResult *runtime_reload_report_domain(RuntimeReloadReport *report, uint32_t domain_mask)
{
    RuntimeReloadDomainResult *result;
    const RuntimeReloadDomainSpec *spec;
    uint8_t i;

    if (report == NULL) {
        return NULL;
    }
    for (i = 0U; i < report->domain_result_count; ++i) {
        if (report->domain_results[i].domain_mask == domain_mask) {
            return &report->domain_results[i];
        }
    }
    if (report->domain_result_count >= RUNTIME_RELOAD_REPORT_DOMAIN_CAPACITY) {
        return NULL;
    }
    result = &report->domain_results[report->domain_result_count++];
    memset(result, 0, sizeof(*result));
    spec = runtime_reload_find_domain_spec(domain_mask, report);
    result->domain_mask = domain_mask;
    result->domain_name = runtime_reload_domain_name(domain_mask);
    result->apply_strategy = spec != NULL ? runtime_reload_apply_strategy_name(spec->strategy) : "UNSUPPORTED";
    result->supported = spec != NULL;
    result->verify_reason = kVerifyReasonNotRequested;
    return result;
}

static void runtime_reload_prepare_domain_results(uint32_t requested_domain_mask, RuntimeReloadReport *report)
{
    uint32_t bit;

    for (bit = 1U; bit != 0U; bit <<= 1U) {
        RuntimeReloadDomainResult *result;
        const RuntimeReloadDomainSpec *spec;

        if ((requested_domain_mask & bit) == 0U) {
            continue;
        }
        result = runtime_reload_report_domain(report, bit);
        if (result == NULL) {
            continue;
        }
        spec = runtime_reload_find_domain_spec(bit, report);
        result->requested = true;
        result->supported = spec != NULL;
        result->apply_strategy = spec != NULL ? runtime_reload_apply_strategy_name(spec->strategy) : "UNSUPPORTED";
        if (spec != NULL && spec->strategy != RUNTIME_RELOAD_APPLY_HOT) {
            result->dispatch_matched = true;
        }
    }
}

static void runtime_reload_note_dispatch_failure(RuntimeReloadReport *report, uint32_t requested_domain_mask)
{
    uint8_t i;

    if (report == NULL) {
        return;
    }
    for (i = 0U; i < report->domain_result_count; ++i) {
        RuntimeReloadDomainResult *result = &report->domain_results[i];
        const RuntimeReloadDomainSpec *spec = runtime_reload_find_domain_spec(result->domain_mask, report);
        if ((requested_domain_mask & result->domain_mask) == 0U || spec == NULL || spec->strategy != RUNTIME_RELOAD_APPLY_HOT) {
            continue;
        }
        result->applied = false;
        result->verify_ok = false;
        result->effective = false;
        result->verify_reason = kVerifyReasonDispatchFailed;
    }
    if ((requested_domain_mask & RUNTIME_RELOAD_DOMAIN_WIFI) != 0U) {
        report->wifi_reload_ok = false;
        report->wifi_verify_reason = kVerifyReasonDispatchFailed;
    }
    if ((requested_domain_mask & RUNTIME_RELOAD_DOMAIN_NETWORK) != 0U) {
        report->network_reload_ok = false;
        report->network_verify_reason = kVerifyReasonDispatchFailed;
    }
}

static bool runtime_reload_preflight_mask(uint32_t requested_domain_mask, RuntimeReloadReport *report)
{
    const uint32_t supported_mask = runtime_reload_supported_domain_mask();
    const uint32_t hot_apply_requested_mask = requested_domain_mask & runtime_reload_domain_mask_for_strategy(RUNTIME_RELOAD_APPLY_HOT);
    const uint8_t capacity = runtime_event_service_capacity();
    const uint8_t handlers = runtime_event_service_handler_count();
    const uint8_t matched = runtime_event_service_matching_handler_count(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
                                                                         hot_apply_requested_mask);
    uint32_t bit;

    if (report != NULL) {
        report->handler_count = handlers;
        report->matched_handler_count = matched;
        report->requested_domain_mask = requested_domain_mask;
    }

    if (requested_domain_mask == RUNTIME_RELOAD_DOMAIN_NONE) {
        return true;
    }
    if ((requested_domain_mask & ~supported_mask) != 0U) {
        runtime_reload_set_failure(report, "PREFLIGHT", "UNSUPPORTED_DOMAIN_REQUESTED",
                                   requested_domain_mask & ~supported_mask);
        return false;
    }
    if (hot_apply_requested_mask == RUNTIME_RELOAD_DOMAIN_NONE) {
        return true;
    }
    if (capacity == 0U) {
        runtime_reload_set_failure(report, "PREFLIGHT", "NO_HANDLER_CAPACITY", hot_apply_requested_mask);
        return false;
    }
    if (handlers == 0U) {
        runtime_reload_set_failure(report, "PREFLIGHT", "NO_EVENT_SUBSCRIBERS", hot_apply_requested_mask);
        return false;
    }
    if (handlers > capacity) {
        runtime_reload_set_failure(report, "PREFLIGHT", "HANDLER_COUNT_EXCEEDS_CAPACITY", hot_apply_requested_mask);
        return false;
    }
    if (matched == 0U) {
        runtime_reload_set_failure(report, "PREFLIGHT", "NO_DOMAIN_SUBSCRIBERS", hot_apply_requested_mask);
        return false;
    }

    for (bit = 1U; bit != 0U; bit <<= 1U) {
        RuntimeReloadDomainResult *result;
        const RuntimeReloadDomainSpec *spec;
        uint8_t domain_handlers;

        if ((requested_domain_mask & bit) == 0U) {
            continue;
        }
        result = runtime_reload_report_domain(report, bit);
        spec = runtime_reload_find_domain_spec(bit, report);
        if (spec == NULL || spec->strategy != RUNTIME_RELOAD_APPLY_HOT) {
            continue;
        }
        domain_handlers = runtime_event_service_matching_handler_count(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED, bit);
        if (result != NULL) {
            result->dispatch_matched = domain_handlers > 0U;
            result->verify_reason = domain_handlers > 0U ? kVerifyReasonNotRequested : kVerifyReasonNoHandler;
        }
        if (domain_handlers == 0U) {
            runtime_reload_set_failure(report, "PREFLIGHT", "DOMAIN_SUBSCRIBER_MISSING", bit);
            return false;
        }
    }
    return true;
}

static void runtime_reload_update_legacy_report(const RuntimeReloadDomainSpec *spec,
                                                uint32_t applied_generation,
                                                bool domain_ok)
{
    if (spec->legacy_ok != NULL) {
        *spec->legacy_ok = domain_ok;
    }
    if (spec->legacy_generation != NULL) {
        *spec->legacy_generation = applied_generation;
    }
    if (spec->legacy_reason != NULL) {
        *spec->legacy_reason = domain_ok ? spec->verify_reason_ok : spec->verify_reason_fail;
    }
}

static bool runtime_reload_verify_mask(uint32_t requested_domain_mask, RuntimeReloadReport *report)
{
    bool ok = true;
    DeviceConfigSnapshot cfg = {0};
    const bool cfg_ok = device_config_get(&cfg);
    const uint32_t generation = device_config_generation();
    size_t spec_count = 0U;
    const RuntimeReloadDomainSpec *specs = runtime_reload_domain_specs(report, &spec_count);
    size_t i;

    if (report != NULL) {
        report->config_generation = generation;
    }

    if (requested_domain_mask != 0U && (!cfg_ok || generation == 0U)) {
        runtime_reload_set_failure(report, "VERIFY", "VERIFY_CONFIG_UNAVAILABLE", requested_domain_mask);
        for (i = 0U; report != NULL && i < report->domain_result_count; ++i) {
            report->domain_results[i].verify_ok = false;
            report->domain_results[i].effective = false;
            report->domain_results[i].verify_reason = "CONFIG_UNAVAILABLE";
        }
        if (report != NULL) {
            report->wifi_reload_ok = false;
            report->network_reload_ok = false;
            report->wifi_verify_reason = "CONFIG_UNAVAILABLE";
            report->network_verify_reason = "CONFIG_UNAVAILABLE";
        }
        return false;
    }

    for (i = 0U; i < spec_count; ++i) {
        const RuntimeReloadDomainSpec *spec = &specs[i];
        RuntimeReloadDomainResult *result;
        uint32_t applied_generation;

        if ((requested_domain_mask & spec->domain_mask) == 0U) {
            continue;
        }
        result = runtime_reload_report_domain(report, spec->domain_mask);
        applied_generation = spec->applied_generation != NULL ? spec->applied_generation() : generation;
        if (result != NULL) {
            result->applied_generation = applied_generation;
            result->apply_strategy = runtime_reload_apply_strategy_name(spec->strategy);
        }

        switch (spec->strategy) {
            case RUNTIME_RELOAD_APPLY_HOT: {
                const bool domain_ok = spec->verify != NULL ? spec->verify(generation, &cfg) : true;
                if (result != NULL) {
                    result->applied = domain_ok;
                    result->verify_ok = domain_ok;
                    result->effective = domain_ok;
                    result->verify_reason = domain_ok ? spec->verify_reason_ok : spec->verify_reason_fail;
                }
                runtime_reload_update_legacy_report(spec, applied_generation, domain_ok);
                if (domain_ok) {
                    if (report != NULL) {
                        report->applied_domain_mask |= spec->domain_mask;
                    }
                } else {
                    ok = false;
                    runtime_reload_set_failure(report, "VERIFY", spec->verify_failure_code, spec->domain_mask);
                }
                break;
            }
            case RUNTIME_RELOAD_APPLY_PERSISTED_ONLY:
                if (result != NULL) {
                    result->applied = false;
                    result->verify_ok = true;
                    result->persisted_only = true;
                    result->effective = true;
                    result->verify_reason = kVerifyReasonPersistedOnly;
                }
                if (report != NULL) {
                    report->deferred_domain_mask |= spec->domain_mask;
                }
                break;
            case RUNTIME_RELOAD_APPLY_REQUIRES_REBOOT:
                if (result != NULL) {
                    result->applied = false;
                    result->verify_ok = true;
                    result->reboot_required = true;
                    result->effective = true;
                    result->verify_reason = kVerifyReasonRequiresReboot;
                }
                if (report != NULL) {
                    report->reboot_required_domain_mask |= spec->domain_mask;
                    report->requires_reboot = true;
                }
                break;
            default:
                ok = false;
                runtime_reload_set_failure(report, "VERIFY", "UNKNOWN_APPLY_STRATEGY", spec->domain_mask);
                if (result != NULL) {
                    result->verify_ok = false;
                    result->effective = false;
                    result->verify_reason = "UNKNOWN_APPLY_STRATEGY";
                }
                break;
        }
    }

    return ok;
}

static bool runtime_reload_execute(uint32_t requested_domain_mask, RuntimeReloadReport *out)
{
    const bool requested = requested_domain_mask != RUNTIME_RELOAD_DOMAIN_NONE;
    const uint32_t hot_apply_requested_mask = requested_domain_mask & runtime_reload_domain_mask_for_strategy(RUNTIME_RELOAD_APPLY_HOT);
    RuntimeReloadReport report = {0};
    RuntimeEventDispatchReport dispatch = {0};
    bool reload_ok = true;

    runtime_reload_report_init(&report, requested);
    report.requested_domain_mask = requested_domain_mask;
    runtime_reload_prepare_domain_results(requested_domain_mask, &report);
    if (!requested) {
        runtime_reload_report_copy(out, &report);
        return true;
    }

    report.preflight_ok = runtime_reload_preflight_mask(requested_domain_mask, &report);
    report.apply_attempted = report.preflight_ok;
    if (!report.preflight_ok) {
        runtime_reload_report_copy(out, &report);
        return false;
    }

    if (hot_apply_requested_mask != RUNTIME_RELOAD_DOMAIN_NONE) {
        reload_ok = runtime_event_service_publish_domains(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED, hot_apply_requested_mask);
        report.event_dispatch_ok = reload_ok;
        report.handler_success_count = runtime_event_service_last_success_count();
        report.handler_failure_count = runtime_event_service_last_failure_count();
        report.handler_critical_failure_count = runtime_event_service_last_critical_failure_count();
        if (runtime_event_service_get_last_dispatch(&dispatch)) {
            report.handler_count = dispatch.handler_count;
            report.matched_handler_count = dispatch.matched_handler_count;
            if (dispatch.first_failed_handler_index >= 0) {
                report.first_failed_handler_name = runtime_event_service_handler_name((uint8_t)dispatch.first_failed_handler_index);
            }
        }
    } else {
        report.event_dispatch_ok = true;
    }

    if (!reload_ok) {
        runtime_reload_set_failure(&report, "DISPATCH", "EVENT_DISPATCH_FAILED", hot_apply_requested_mask);
        report.verify_ok = false;
        runtime_reload_note_dispatch_failure(&report, hot_apply_requested_mask);
        report.partial_success = requested && report.handler_success_count > 0U;
        runtime_reload_report_copy(out, &report);
        return false;
    }

    report.verify_ok = runtime_reload_verify_mask(requested_domain_mask, &report);
    reload_ok = report.verify_ok;
    report.partial_success = report.failed_domain_mask != RUNTIME_RELOAD_DOMAIN_NONE &&
                             (report.applied_domain_mask != RUNTIME_RELOAD_DOMAIN_NONE ||
                              report.deferred_domain_mask != RUNTIME_RELOAD_DOMAIN_NONE ||
                              report.reboot_required_domain_mask != RUNTIME_RELOAD_DOMAIN_NONE);
    runtime_reload_report_copy(out, &report);
    return reload_ok;
}

static bool runtime_reload_preflight(bool wifi_changed,
                                     bool network_changed,
                                     RuntimeReloadReport *report)
{
    return runtime_reload_preflight_mask(runtime_reload_requested_from_legacy(wifi_changed, network_changed), report);
}

static bool runtime_reload_verify(bool wifi_changed,
                                  bool network_changed,
                                  RuntimeReloadReport *report)
{
    return runtime_reload_verify_mask(runtime_reload_requested_from_legacy(wifi_changed, network_changed), report);
}

bool runtime_reload_device_config_domains(uint32_t requested_domain_mask,
                                          RuntimeReloadReport *out)
{
    return runtime_reload_execute(requested_domain_mask, out);
}

bool runtime_reload_device_config(bool wifi_changed,
                                  bool network_changed,
                                  RuntimeReloadReport *out)
{
    return runtime_reload_execute(runtime_reload_requested_from_legacy(wifi_changed, network_changed), out);
}

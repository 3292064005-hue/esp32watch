#include "services/runtime_reload_coordinator.h"
#include "services/runtime_event_service.h"
#include "services/network_sync_service.h"
#include "services/device_config.h"
#include "web/web_wifi.h"
#include <string.h>

static uint32_t runtime_reload_requested_domain_mask(bool wifi_changed, bool network_changed)
{
    uint32_t mask = RUNTIME_RELOAD_DOMAIN_NONE;
    if (wifi_changed) {
        mask |= RUNTIME_RELOAD_DOMAIN_WIFI;
    }
    if (network_changed) {
        mask |= RUNTIME_RELOAD_DOMAIN_NETWORK;
    }
    return mask;
}

static void runtime_reload_mark_failed_domain(RuntimeReloadReport *report, uint32_t domain_mask)
{
    if (report != NULL) {
        report->failed_domain_mask |= domain_mask;
    }
}

static void runtime_reload_mark_applied_domain(RuntimeReloadReport *report, uint32_t domain_mask)
{
    if (report != NULL) {
        report->applied_domain_mask |= domain_mask;
    }
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
    out->wifi_verify_reason = "NONE";
    out->network_verify_reason = "NONE";
}

static void runtime_reload_report_copy(RuntimeReloadReport *dst, const RuntimeReloadReport *src)
{
    if (dst == NULL || src == NULL) {
        return;
    }
    *dst = *src;
}

static bool runtime_reload_preflight(bool wifi_changed,
                                     bool network_changed,
                                     RuntimeReloadReport *report)
{
    const uint8_t capacity = runtime_event_service_capacity();
    const uint8_t handlers = runtime_event_service_handler_count();
    const uint32_t requested_domains = runtime_reload_requested_domain_mask(wifi_changed, network_changed);

    if (report != NULL) {
        report->handler_count = handlers;
        report->requested_domain_mask = requested_domains;
    }

    if (!wifi_changed && !network_changed) {
        return true;
    }
    if (capacity == 0U) {
        runtime_reload_set_failure(report, "PREFLIGHT", "NO_HANDLER_CAPACITY", requested_domains);
        return false;
    }
    if (handlers == 0U) {
        runtime_reload_set_failure(report, "PREFLIGHT", "NO_EVENT_SUBSCRIBERS", requested_domains);
        return false;
    }
    if (handlers > capacity) {
        runtime_reload_set_failure(report, "PREFLIGHT", "HANDLER_COUNT_EXCEEDS_CAPACITY", requested_domains);
        return false;
    }
    return true;
}

static bool runtime_reload_verify(bool wifi_changed, bool network_changed, RuntimeReloadReport *report)
{
    bool ok = true;
    DeviceConfigSnapshot cfg = {0};
    const bool cfg_ok = device_config_get(&cfg);
    const uint32_t generation = device_config_generation();

    if (report != NULL) {
        report->config_generation = generation;
    }

    if ((wifi_changed || network_changed) && (!cfg_ok || generation == 0U)) {
        runtime_reload_set_failure(report, "VERIFY", "VERIFY_CONFIG_UNAVAILABLE",
                                   runtime_reload_requested_domain_mask(wifi_changed, network_changed));
        if (wifi_changed) {
            report->wifi_reload_ok = false;
            report->wifi_verify_reason = "CONFIG_UNAVAILABLE";
            report->failed_domain_mask |= RUNTIME_RELOAD_DOMAIN_WIFI;
        }
        if (network_changed) {
            report->network_reload_ok = false;
            report->network_verify_reason = "CONFIG_UNAVAILABLE";
            report->failed_domain_mask |= RUNTIME_RELOAD_DOMAIN_NETWORK;
        }
        return false;
    }

    if (wifi_changed) {
        const bool wifi_ok = web_wifi_verify_config_applied(generation, &cfg);
        if (report != NULL) {
            report->wifi_applied_generation = web_wifi_last_applied_generation();
            report->wifi_reload_ok = wifi_ok;
            report->wifi_verify_reason = wifi_ok ? "CONFIG_APPLIED" : "CONFIG_MISMATCH";
        }
        ok = ok && wifi_ok;
        if (wifi_ok) {
            runtime_reload_mark_applied_domain(report, RUNTIME_RELOAD_DOMAIN_WIFI);
        } else {
            runtime_reload_mark_failed_domain(report, RUNTIME_RELOAD_DOMAIN_WIFI);
            runtime_reload_set_failure(report, "VERIFY", "VERIFY_WIFI_CONFIG_MISMATCH", RUNTIME_RELOAD_DOMAIN_WIFI);
        }
    }

    if (network_changed) {
        const bool network_ok = network_sync_service_verify_config_applied(generation, &cfg);
        if (report != NULL) {
            report->network_applied_generation = network_sync_service_last_applied_generation();
            report->network_reload_ok = network_ok;
            report->network_verify_reason = network_ok ? "CONFIG_APPLIED" : "CONFIG_MISMATCH";
        }
        ok = ok && network_ok;
        if (network_ok) {
            runtime_reload_mark_applied_domain(report, RUNTIME_RELOAD_DOMAIN_NETWORK);
        } else {
            runtime_reload_mark_failed_domain(report, RUNTIME_RELOAD_DOMAIN_NETWORK);
            runtime_reload_set_failure(report, "VERIFY", "VERIFY_NETWORK_CONFIG_MISMATCH", RUNTIME_RELOAD_DOMAIN_NETWORK);
        }
    }

    return ok;
}

bool runtime_reload_device_config(bool wifi_changed,
                                  bool network_changed,
                                  RuntimeReloadReport *out)
{
    const bool requested = wifi_changed || network_changed;
    RuntimeReloadReport report = {};
    RuntimeEventDispatchReport dispatch = {};
    bool reload_ok = true;

    runtime_reload_report_init(&report, requested);
    report.requested_domain_mask = runtime_reload_requested_domain_mask(wifi_changed, network_changed);
    if (!requested) {
        runtime_reload_report_copy(out, &report);
        return true;
    }

    report.preflight_ok = runtime_reload_preflight(wifi_changed, network_changed, &report);
    report.apply_attempted = report.preflight_ok;
    if (!report.preflight_ok) {
        runtime_reload_report_copy(out, &report);
        return false;
    }

    reload_ok = runtime_event_service_publish(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
    report.event_dispatch_ok = reload_ok;
    report.handler_success_count = runtime_event_service_last_success_count();
    report.handler_failure_count = runtime_event_service_last_failure_count();
    report.handler_critical_failure_count = runtime_event_service_last_critical_failure_count();
    if (runtime_event_service_get_last_dispatch(&dispatch)) {
        report.handler_count = dispatch.handler_count;
        if (dispatch.first_failed_handler_index >= 0) {
            report.first_failed_handler_name = runtime_event_service_handler_name((uint8_t)dispatch.first_failed_handler_index);
        }
    }

    if (!reload_ok) {
        runtime_reload_set_failure(&report, "DISPATCH", "EVENT_DISPATCH_FAILED", report.requested_domain_mask);
        report.verify_ok = false;
        report.wifi_reload_ok = !wifi_changed;
        report.network_reload_ok = !network_changed;
        report.partial_success = requested && report.handler_success_count > 0U;
        runtime_reload_report_copy(out, &report);
        return false;
    }

    report.verify_ok = runtime_reload_verify(wifi_changed, network_changed, &report);
    reload_ok = report.verify_ok;
    report.partial_success = requested && report.applied_domain_mask != 0U &&
                             ((report.applied_domain_mask & report.requested_domain_mask) != report.requested_domain_mask);
    runtime_reload_report_copy(out, &report);
    return reload_ok;
}

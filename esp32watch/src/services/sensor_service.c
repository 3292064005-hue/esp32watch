#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/storage_facade.h"
#include "services/runtime_event_service.h"
#include "services/runtime_reload_coordinator.h"
#include "model.h"
#include <stddef.h>

static bool g_sensor_service_initialized;
static uint32_t g_last_applied_generation;

static void sensor_service_note_applied_generation(void)
{
    g_last_applied_generation = device_config_generation();
}

static bool sensor_service_handle_runtime_event(RuntimeServiceEvent event, void *ctx)
{
    ModelDomainState domain_state = {0};
    (void)ctx;
    if (event != RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED) {
        return true;
    }
    if (!g_sensor_service_initialized || model_get_domain_state(&domain_state) == NULL) {
        return false;
    }
    sensor_service_set_sensitivity(domain_state.settings.sensor_sensitivity);
    sensor_service_note_applied_generation();
    return true;
}

void sensor_service_init(void)
{
    SensorCalibrationData cal;
    RuntimeEventSubscription subscription = {
        .handler = sensor_service_handle_runtime_event,
        .ctx = NULL,
        .name = "sensor_service",
        .priority = 5,
        .critical = true,
        .event_mask = runtime_event_service_event_mask(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED),
        .domain_mask = RUNTIME_RELOAD_DOMAIN_SENSOR,
    };

    sensor_init();
    storage_facade_load_sensor_calibration(&cal);
    sensor_load_calibration(&cal);
    g_sensor_service_initialized = sensor_get_snapshot() != NULL && runtime_event_service_register_ex(&subscription);
    if (g_sensor_service_initialized) {
        sensor_service_note_applied_generation();
    }
}

void sensor_service_tick(uint32_t now_ms)
{
    SensorCalibrationData cal;
    sensor_tick(now_ms);
    if (sensor_take_calibration_dirty(&cal)) {
        storage_facade_save_sensor_calibration(&cal);
        storage_service_request_commit(STORAGE_COMMIT_REASON_CALIBRATION);
    }
}

const SensorSnapshot *sensor_service_get_snapshot(void)
{
    return sensor_get_snapshot();
}

bool sensor_service_force_reinit(void)
{
    return sensor_force_reinit();
}

void sensor_service_request_calibration(void)
{
    sensor_request_calibration();
}

void sensor_service_clear_calibration(void)
{
    sensor_clear_calibration();
}

void sensor_service_set_sensitivity(uint8_t level)
{
    sensor_set_sensitivity(level);
}

void sensor_service_note_storage_commit_result(StorageCommitReason reason, bool ok)
{
    if (reason != STORAGE_COMMIT_REASON_CALIBRATION) {
        return;
    }
    if (ok) {
        sensor_note_calibration_persisted();
    } else {
        sensor_note_calibration_persist_failed();
    }
}

SensorRuntimeState sensor_service_get_runtime_state(void)
{
    return sensor_get_snapshot()->runtime_state;
}

SensorCalibrationState sensor_service_get_calibration_state(void)
{
    return sensor_get_snapshot()->calibration_state;
}

const char *sensor_service_runtime_state_name(void)
{
    return sensor_runtime_state_name(sensor_service_get_runtime_state());
}

const char *sensor_service_calibration_state_name(void)
{
    return sensor_calibration_state_name(sensor_service_get_calibration_state());
}


bool sensor_service_is_initialized(void)
{
    return g_sensor_service_initialized;
}

uint32_t sensor_service_last_applied_generation(void)
{
    return g_last_applied_generation;
}

bool sensor_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_sensor_service_initialized && generation != 0U && g_last_applied_generation == generation;
}

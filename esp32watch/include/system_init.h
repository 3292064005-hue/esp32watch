#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSTEM_INIT_STAGE_NONE = 0,
    SYSTEM_INIT_STAGE_HAL,
    SYSTEM_INIT_STAGE_RESET_REASON,
    SYSTEM_INIT_STAGE_CLOCK,
    SYSTEM_INIT_STAGE_GPIO,
    SYSTEM_INIT_STAGE_UART,
    SYSTEM_INIT_STAGE_BKP,
    SYSTEM_INIT_STAGE_RTC,
    SYSTEM_INIT_STAGE_I2C,
    SYSTEM_INIT_STAGE_ADC,
    SYSTEM_INIT_STAGE_IWDG,
    SYSTEM_INIT_STAGE_CAPABILITY_PROBE,
    SYSTEM_INIT_STAGE_TIME_SERVICE,
    SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE,
    SYSTEM_INIT_STAGE_STORAGE_SERVICE,
    SYSTEM_INIT_STAGE_WEB_SERVER,
    SYSTEM_INIT_STAGE_COMPANION_TRANSPORT,
    SYSTEM_INIT_STAGE_APP
} SystemInitStage;

typedef enum {
    SYSTEM_FATAL_CODE_NONE = 0,
    SYSTEM_FATAL_CODE_INIT_STAGE_FAILURE,
    SYSTEM_FATAL_CODE_CAPABILITY_CONTRACT
} SystemFatalCode;

typedef enum {
    SYSTEM_FATAL_POLICY_STOP = 0,
    SYSTEM_FATAL_POLICY_SAFE_MODE
} SystemFatalPolicy;

typedef enum {
    SYSTEM_INIT_STAGE_STATUS_NOT_RUN = 0,
    SYSTEM_INIT_STAGE_STATUS_OK,
    SYSTEM_INIT_STAGE_STATUS_DEGRADED,
    SYSTEM_INIT_STAGE_STATUS_FAILED
} SystemInitStageStatus;

typedef enum {
    SYSTEM_RUNTIME_INIT_PROFILE_STORAGE_AUTHORITY_FIRST = 0,
    SYSTEM_RUNTIME_INIT_PROFILE_LEGACY_CONSUMER_FIRST
} SystemRuntimeInitProfile;

typedef struct {
    bool has_battery_adc;
    bool has_vibration;
    bool has_sensor;
    bool has_watchdog;
    bool has_flash_storage;
    bool has_bkp_mirror;
    bool has_full_keypad;
    bool keypad_mapping_valid;
    bool gpio_contract_valid;
    bool has_idle_light_sleep;
    bool has_reset_domain_storage;
    bool storage_map_valid;
} SystemRuntimeCapabilities;

typedef struct {
    bool startup_ok;
    bool degraded;
    bool fatal_stop_requested;
    SystemInitStage failure_stage;
    SystemInitStage recovery_stage;
    SystemInitStage last_completed_stage;
    SystemFatalCode fatal_code;
    SystemFatalPolicy fatal_policy;
    uint32_t fatal_detail;
} SystemStartupReport;

/**
 * @brief Compatibility view of the normalized startup report.
 *
 * Older runtime telemetry paths still consume these legacy field names. The
 * data is derived from the authoritative @ref SystemStartupReport snapshot.
 */
typedef struct {
    bool init_failed;
    SystemInitStage last_stage;
    bool safe_mode_boot_recovery_pending;
    SystemInitStage safe_mode_boot_recovery_stage;
    SystemFatalCode fatal_code;
    SystemFatalPolicy fatal_policy;
    uint32_t fatal_detail;
} SystemStartupStatus;

/**
 * @brief Run chip bootstrap stages that must execute before peripheral bring-up.
 *
 * Resets the startup transaction state, initializes platform prerequisites, and
 * records the bootstrapping stages that must succeed before later service or UI
 * layers are allowed to start.
 *
 * @return void
 */
void system_bootstrap(void);

/**
 * @brief Initialize board peripherals in the agreed startup order.
 *
 * The function executes the full peripheral bring-up transaction. Any
 * non-recoverable failure raises the system fatal handler and returns false.
 * Recoverable stages that support same-boot safe-mode degradation are marked as
 * degraded and still allow startup to continue.
 *
 * @return true when all required peripheral stages completed or were
 *         recoverably degraded; false when a fatal stop path was triggered.
 */
bool system_board_peripheral_init(void);

/**
 * @brief Initialize runtime services that must come online before watch_app_init().
 *
 * This call is part of the authoritative startup transaction and must succeed
 * before the application layer is allowed to initialize. The active startup
 * profile defines whether authority services lead or whether legacy consumer-
 * first fallback ordering is in effect.
 *
 * @return true when all required runtime services were initialized
 *         successfully; false when a fatal stop path was triggered.
 */
bool system_runtime_service_init(void);

/**
 * @brief Return the active runtime-service initialization profile.
 *
 * @return Active startup profile controlling runtime service order and rollback policy.
 */
SystemRuntimeInitProfile system_runtime_init_profile(void);

/**
 * @brief Return the stable printable name for the active runtime-service initialization profile.
 *
 * @return Stable profile name string.
 */
const char *system_runtime_init_profile_name(void);

/**
 * @brief Return the number of stages in the active runtime-service startup plan.
 *
 * @return Count of runtime service stages in the current plan.
 */
uint8_t system_runtime_service_plan_length(void);

/**
 * @brief Return the stage assigned to a runtime-service plan slot.
 *
 * @param[in] index Zero-based entry in the current runtime-service plan.
 * @return The requested stage, or SYSTEM_INIT_STAGE_NONE when @p index is out of range.
 */
SystemInitStage system_runtime_service_plan_stage(uint8_t index);

/**
 * @brief Check whether the active runtime-service prerequisites are already satisfied for a stage.
 *
 * @param[in] stage Runtime service stage to evaluate.
 * @return true when the current startup transaction already satisfied every prerequisite for @p stage.
 */
bool system_runtime_service_stage_prerequisites_met(SystemInitStage stage);

/**
 * @brief Initialize the web control plane as part of the startup transaction.
 *
 * @return true when the HTTP control plane is ready for runtime use; false when
 *         startup must stop because the web control plane could not be brought up.
 */
bool system_web_service_init(void);

/**
 * @brief Mark the companion transport stage as successfully reached.
 */
void system_mark_companion_transport_initialized(void);

/**
 * @brief Mark the application initialization stage as successfully reached.
 */
void system_mark_app_initialized(void);

/**
 * @brief Report whether runtime mutation routes may accept control actions.
 *
 * Control readiness is stricter than HTTP server readiness: storage, time,
 * network sync, web server, and the application stage must all have completed
 * before AsyncWebServer callbacks may enqueue runtime actions.
 *
 * @return true when runtime control actions may be accepted; false during
 *         startup, degraded fatal stop, or before app initialization completes.
 */
bool system_runtime_control_ready(void);


/**
 * @brief Query the last successful or degraded system initialization stage.
 *
 * @return Last stage completed before any fatal stop or the most recent
 *         degraded stage.
 */
SystemInitStage system_init_last_stage(void);

/**
 * @brief Report whether a startup stage completed successfully during the current boot.
 *
 * @param[in] stage Initialization stage to query.
 * @return true when the stage completed successfully; false when it did not run,
 *         failed, or was only recoverably degraded.
 */
bool system_init_stage_completed(SystemInitStage stage);

/**
 * @brief Report the recorded status for an initialization stage.
 *
 * @param[in] stage Initialization stage to query.
 * @return Recorded startup status for the requested stage.
 */
SystemInitStageStatus system_init_stage_status(SystemInitStage stage);

/**
 * @brief Probe runtime-declared board capabilities and validate manifest invariants.
 *
 * @param[out] out Optional destination for the resolved capability snapshot.
 * @return true when the manifest/capability contract is valid for runtime use.
 */
bool system_runtime_capability_probe(SystemRuntimeCapabilities *out);

/**
 * @brief Report whether startup has entered the fatal initialization trap.
 *
 * @return true when a prior init stage failed and the fatal stop path was entered.
 */
bool system_init_has_failed(void);
SystemFatalCode system_last_fatal_code(void);
SystemFatalPolicy system_last_fatal_policy(void);
uint32_t system_last_fatal_detail(void);
void system_raise_fatal(SystemFatalCode code, SystemInitStage stage, uint32_t detail, SystemFatalPolicy policy);

/**
 * @brief Return the stable printable name for a startup stage.
 *
 * @param[in] stage Initialization stage to stringify.
 * @return Stable stage name string.
 */
const char *system_init_stage_name(SystemInitStage stage);

/**
 * @brief Return the stable printable name for a startup stage status.
 *
 * @param[in] status Stage status to stringify.
 * @return Stable status name string.
 */
const char *system_init_stage_status_name(SystemInitStageStatus status);

/**
 * @brief Return the stable printable name for a fatal startup code.
 *
 * @param[in] code Fatal code to stringify.
 * @return Stable fatal code string.
 */
const char *system_fatal_code_name(SystemFatalCode code);

/**
 * @brief Return the stable printable name for a fatal startup policy.
 *
 * @param[in] policy Fatal policy to stringify.
 * @return Stable fatal policy string.
 */
const char *system_fatal_policy_name(SystemFatalPolicy policy);

/**
 * @brief Copy the current startup transaction report.
 *
 * @param[out] out Destination report buffer.
 * @return true when @p out was populated; false when @p out is NULL.
 */
bool system_get_startup_report(SystemStartupReport *out);

/**
 * @brief Copy the normalized compatibility startup snapshot used by legacy telemetry.
 *
 * @param[out] out Destination status buffer.
 * @return true when @p out was populated; false when @p out is NULL.
 */
bool system_get_startup_status(SystemStartupStatus *out);

/**
 * @brief Report whether the current boot should continue in safe mode after a recoverable init fault.
 *
 * @return true when a recoverable init fault requested same-boot entry in degraded mode.
 */
bool system_safe_mode_boot_recovery_pending(void);

/**
 * @brief Query the init stage that triggered same-boot degraded recovery.
 *
 * @return Stage recorded for the current boot recovery request, or NONE.
 */
SystemInitStage system_safe_mode_boot_recovery_stage(void);

/**
 * @brief Report whether runtime battery sampling remains available after init recovery.
 *
 * @return true when battery ADC support survived startup without degradation.
 */
bool system_runtime_battery_adc_available(void);

/**
 * @brief Report whether the hardware watchdog remains available after init recovery.
 *
 * @return true when watchdog support survived startup without degradation.
 */
bool system_runtime_watchdog_available(void);

/**
 * @brief Prepare the active platform clock domain before peripheral bring-up.
 *
 * On ESP32 this is a normalized hook rather than a direct clock-tree mutator;
 * the Arduino/IDF runtime already owns the underlying clock configuration.
 *
 * @return void
 */
void system_clock_prepare_platform(void);

/**
 * @brief Backward-compatible alias for legacy callers that still expect STM32-style clock setup.
 *
 * The alias forwards to @ref system_clock_prepare_platform so active-path code no
 * longer depends on an empty STM32-named hook.
 *
 * @return void
 */
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif

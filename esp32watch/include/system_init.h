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
    SYSTEM_INIT_STAGE_STORAGE_SERVICE,
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

typedef struct {
    bool has_battery_adc;
    bool has_vibration;
    bool has_sensor;
    bool has_watchdog;
    bool has_flash_storage;
    bool has_bkp_mirror;
    bool storage_map_valid;
} SystemRuntimeCapabilities;

/**
 * @brief Run chip bootstrap stages that must execute before peripheral bring-up.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void system_bootstrap(void);

/**
 * @brief Initialize board peripherals in the agreed startup order.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void system_board_peripheral_init(void);

/**
 * @brief Initialize runtime services that must come online before watch_app_init().
 *
 * @param None.
 * @return void
 * @throws None.
 */
void system_runtime_service_init(void);

/**
 * @brief Mark the companion transport stage as successfully reached.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void system_mark_companion_transport_initialized(void);

/**
 * @brief Mark the application initialization stage as successfully reached.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void system_mark_app_initialized(void);

/**
 * @brief Query the last successful system initialization stage.
 *
 * @param None.
 * @return Last stage reached before any failure trap.
 * @throws None.
 */
SystemInitStage system_init_last_stage(void);

/**
 * @brief Report whether a startup stage completed successfully during the current boot.
 *
 * @param[in] stage Initialization stage to query.
 * @return true when the stage completed successfully; false when it did not run, failed, or was bypassed by safe-mode recovery.
 * @throws None.
 */
bool system_init_stage_completed(SystemInitStage stage);

/**
 * @brief Probe runtime-declared board capabilities and validate manifest invariants.
 *
 * @param[out] out Optional destination for the resolved capability snapshot.
 * @return true when the manifest/capability contract is valid for runtime use.
 * @throws None.
 */
bool system_runtime_capability_probe(SystemRuntimeCapabilities *out);

/**
 * @brief Report whether startup has entered the fatal initialization trap.
 *
 * @param None.
 * @return true when a prior init stage failed and Error_Handler was invoked.
 * @throws None.
 */
bool system_init_has_failed(void);
SystemFatalCode system_last_fatal_code(void);
SystemFatalPolicy system_last_fatal_policy(void);
uint32_t system_last_fatal_detail(void);
void system_raise_fatal(SystemFatalCode code, SystemInitStage stage, uint32_t detail, SystemFatalPolicy policy);

const char *system_init_stage_name(SystemInitStage stage);

/**
 * @brief Report whether the current boot should continue in safe mode after a recoverable init fault.
 *
 * @param None.
 * @return true when a recoverable init fault requested same-boot safe-mode entry.
 * @throws None.
 */
bool system_safe_mode_boot_recovery_pending(void);

/**
 * @brief Query the init stage that triggered same-boot safe-mode recovery.
 *
 * @param None.
 * @return Stage recorded for the current boot recovery request, or NONE.
 * @throws None.
 */
SystemInitStage system_safe_mode_boot_recovery_stage(void);

/**
 * @brief Report whether runtime battery sampling remains available after init recovery.
 *
 * @param None.
 * @return true when battery ADC support survived startup.
 * @throws None.
 */
bool system_runtime_battery_adc_available(void);

/**
 * @brief Report whether the hardware watchdog remains available after init recovery.
 *
 * @param None.
 * @return true when watchdog support survived startup.
 * @throws None.
 */
bool system_runtime_watchdog_available(void);

void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif

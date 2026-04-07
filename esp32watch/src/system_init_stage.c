#include "system_init_stage_internal.h"
#include <string.h>

static bool system_init_stage_is_completable(SystemInitStage stage)
{
    return stage >= SYSTEM_INIT_STAGE_GPIO && stage <= SYSTEM_INIT_STAGE_IWDG;
}

void system_init_completion_reset(SystemInitCompletionState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void system_init_completion_mark(SystemInitCompletionState *state, SystemInitStage stage)
{
    if (state == NULL || !system_init_stage_is_completable(stage)) {
        return;
    }
    state->completed[(uint32_t)stage] = true;
}

bool system_init_completion_is_marked(const SystemInitCompletionState *state, SystemInitStage stage)
{
    if (state == NULL || !system_init_stage_is_completable(stage)) {
        return false;
    }
    return state->completed[(uint32_t)stage];
}

bool system_init_stage_supports_same_boot_safe_mode(SystemInitStage stage)
{
    switch (stage) {
        case SYSTEM_INIT_STAGE_ADC:
        case SYSTEM_INIT_STAGE_IWDG:
            return true;
        default:
            return false;
    }
}

const char *system_init_stage_name_internal(SystemInitStage stage)
{
    switch (stage) {
        case SYSTEM_INIT_STAGE_HAL: return "HAL";
        case SYSTEM_INIT_STAGE_RESET_REASON: return "RESET";
        case SYSTEM_INIT_STAGE_CLOCK: return "CLOCK";
        case SYSTEM_INIT_STAGE_GPIO: return "GPIO";
        case SYSTEM_INIT_STAGE_UART: return "UART";
        case SYSTEM_INIT_STAGE_BKP: return "BKP";
        case SYSTEM_INIT_STAGE_RTC: return "RTC";
        case SYSTEM_INIT_STAGE_I2C: return "I2C";
        case SYSTEM_INIT_STAGE_ADC: return "ADC";
        case SYSTEM_INIT_STAGE_IWDG: return "IWDG";
        case SYSTEM_INIT_STAGE_CAPABILITY_PROBE: return "CAPS";
        case SYSTEM_INIT_STAGE_TIME_SERVICE: return "TIME_SVC";
        case SYSTEM_INIT_STAGE_STORAGE_SERVICE: return "STOR_SVC";
        case SYSTEM_INIT_STAGE_COMPANION_TRANSPORT: return "COMPANION";
        case SYSTEM_INIT_STAGE_APP: return "APP";
        default: return "NONE";
    }
}

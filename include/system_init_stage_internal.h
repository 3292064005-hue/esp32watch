#ifndef SYSTEM_INIT_STAGE_INTERNAL_H
#define SYSTEM_INIT_STAGE_INTERNAL_H

#include "system_init.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool completed[SYSTEM_INIT_STAGE_APP + 1U];
} SystemInitCompletionState;

void system_init_completion_reset(SystemInitCompletionState *state);
void system_init_completion_mark(SystemInitCompletionState *state, SystemInitStage stage);
bool system_init_completion_is_marked(const SystemInitCompletionState *state, SystemInitStage stage);
bool system_init_stage_supports_same_boot_safe_mode(SystemInitStage stage);
const char *system_init_stage_name_internal(SystemInitStage stage);

#ifdef __cplusplus
}
#endif

#endif

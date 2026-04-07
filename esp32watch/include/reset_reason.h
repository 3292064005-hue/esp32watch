#ifndef RESET_REASON_H
#define RESET_REASON_H

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

void reset_reason_init(void);
ResetReason reset_reason_get(void);
const char *reset_reason_name(ResetReason reason);

#ifdef __cplusplus
}
#endif

#endif

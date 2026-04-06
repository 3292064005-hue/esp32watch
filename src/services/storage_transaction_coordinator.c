#include "services/storage_service_internal.h"
#include <string.h>

void storage_tx_begin(StorageServiceState *state)
{
    if (state == NULL) {
        return;
    }
    state->transaction_active = true;
    state->transaction_reason = STORAGE_COMMIT_REASON_NONE;
}

void storage_tx_finalize(StorageServiceState *state, bool commit, StorageCommitReason reason)
{
    if (state == NULL || !state->transaction_active) {
        return;
    }

    state->transaction_active = false;
    state->commit_in_progress = false;
    state->transaction_reason = commit ? reason : STORAGE_COMMIT_REASON_NONE;
    if (!commit) {
        state->requested_commit_reason = STORAGE_COMMIT_REASON_NONE;
    }
}

void storage_tx_abort(StorageServiceState *state)
{
    storage_tx_finalize(state, false, STORAGE_COMMIT_REASON_NONE);
}

bool storage_tx_prepare_commit(StorageServiceState *state, StorageCommitReason reason, StorageCommitExecutionContext *ctx)
{
    if (state == NULL || ctx == NULL) {
        return false;
    }
    if (!storage_scheduler_has_pending(state) || state->commit_in_progress) {
        memset(ctx, 0, sizeof(*ctx));
        return false;
    }

    ctx->previously_transaction_active = state->transaction_active;
    state->commit_in_progress = true;
    state->transaction_active = true;
    state->transaction_reason = reason;
    return true;
}

void storage_tx_finish_commit(StorageServiceState *state, const StorageCommitExecutionContext *ctx, bool ok)
{
    if (state == NULL || ctx == NULL) {
        return;
    }

    state->transaction_active = ctx->previously_transaction_active;
    state->commit_in_progress = false;
    if (!ok) {
        state->transaction_reason = STORAGE_COMMIT_REASON_NONE;
    }
}

#include "companion_proto_contract.h"

#include <string.h>

#include "companion_proto_internal.h"

static const char *const kCompanionProtoGetSubjects[] = {
    "INFO",
    "SETTINGS",
    "DIAG",
    "FAULTS",
    "SENSOR",
    "ACTIVITY",
    "STORAGE",
    "CLOCK",
    "PERF",
    "PROTO",
};

static const char *const kCompanionProtoExportSubjects[] = {
    "STORAGE",
    "ACTIVITY",
    "DIAG",
    "CLOCK",
    "PERF",
    "PROTO",
};

static const char *kCompanionProtoCapsCsv =
    "info,settings,diag,faults,sensor,activity,storage,clock,perf,safeclr,sensorreinit";

static bool companion_proto_contract_contains(const char *candidate,
                                              const char *const *items,
                                              size_t count)
{
    size_t i;

    if (candidate == NULL || items == NULL) {
        return false;
    }

    for (i = 0U; i < count; ++i) {
        if (companion_proto_token_eq(candidate, items[i])) {
            return true;
        }
    }
    return false;
}

uint32_t companion_proto_contract_version(void)
{
    return COMPANION_PROTO_VERSION;
}

const char *companion_proto_contract_caps_csv(void)
{
    return kCompanionProtoCapsCsv;
}

bool companion_proto_contract_supports_get_subject(const char *subject)
{
    return companion_proto_contract_contains(subject,
                                             kCompanionProtoGetSubjects,
                                             sizeof(kCompanionProtoGetSubjects) / sizeof(kCompanionProtoGetSubjects[0]));
}

bool companion_proto_contract_supports_export_subject(const char *subject)
{
    return companion_proto_contract_contains(subject,
                                             kCompanionProtoExportSubjects,
                                             sizeof(kCompanionProtoExportSubjects) / sizeof(kCompanionProtoExportSubjects[0]));
}

size_t companion_proto_contract_get_subjects(const char *const **out)
{
    if (out != NULL) {
        *out = kCompanionProtoGetSubjects;
    }
    return sizeof(kCompanionProtoGetSubjects) / sizeof(kCompanionProtoGetSubjects[0]);
}

size_t companion_proto_contract_export_subjects(const char *const **out)
{
    if (out != NULL) {
        *out = kCompanionProtoExportSubjects;
    }
    return sizeof(kCompanionProtoExportSubjects) / sizeof(kCompanionProtoExportSubjects[0]);
}

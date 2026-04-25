#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "companion_proto_contract.h"

int main(void)
{
    const char *const *subjects = NULL;
    size_t count = companion_proto_contract_get_subjects(&subjects);

    assert(companion_proto_contract_version() == 4U);
    assert(strcmp(companion_proto_contract_caps_csv(), "info,settings,diag,faults,sensor,activity,storage,clock,perf,safeclr,sensorreinit") == 0);
    assert(count >= 1U);
    assert(subjects != NULL);
    assert(companion_proto_contract_supports_get_subject("INFO"));
    assert(companion_proto_contract_supports_get_subject("proto"));
    assert(!companion_proto_contract_supports_get_subject("MISSING"));
    assert(companion_proto_contract_supports_export_subject("PROTO"));
    assert(!companion_proto_contract_supports_export_subject("SETTINGS"));

    puts("[OK] companion proto contract check passed");
    return 0;
}

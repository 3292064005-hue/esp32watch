#ifndef COMPANION_PROTO_CONTRACT_H
#define COMPANION_PROTO_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the protocol contract version exposed to companion clients.
 *
 * @return Stable companion protocol contract version.
 * @throws None.
 */
uint32_t companion_proto_contract_version(void);

/**
 * @brief Return the canonical capability CSV exposed by the companion protocol.
 *
 * @return Comma-separated capability list owned by the protocol contract.
 * @throws None.
 */
const char *companion_proto_contract_caps_csv(void);

/**
 * @brief Test whether a GET subject is part of the canonical companion contract.
 *
 * @param[in] subject Candidate subject token.
 * @return true when the subject is supported by the GET surface.
 * @throws None.
 */
bool companion_proto_contract_supports_get_subject(const char *subject);

/**
 * @brief Test whether an EXPORT subject is part of the canonical companion contract.
 *
 * @param[in] subject Candidate subject token.
 * @return true when the subject is supported by the EXPORT surface.
 * @throws None.
 */
bool companion_proto_contract_supports_export_subject(const char *subject);

/**
 * @brief Copy the GET subject catalog into a caller-supplied buffer.
 *
 * @param[out] out Destination pointer to the internal immutable subject array.
 * @return Number of subjects in the GET catalog.
 * @throws None.
 */
size_t companion_proto_contract_get_subjects(const char *const **out);

/**
 * @brief Copy the EXPORT subject catalog into a caller-supplied buffer.
 *
 * @param[out] out Destination pointer to the internal immutable subject array.
 * @return Number of subjects in the EXPORT catalog.
 * @throws None.
 */
size_t companion_proto_contract_export_subjects(const char *const **out);

#ifdef __cplusplus
}
#endif

#endif

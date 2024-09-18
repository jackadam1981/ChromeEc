// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform.h"
#include <string.h>

// Calculate UDS
bool __platform_get_uds(
    platform_context_t ctx, // [IN] opaque platform context
    uds_mut_t uds // [OUT] UDS
) {
    memset(uds, 0, UDS_BYTES);
    return true;
}

// Perform HKDF-SHA256(ikm, salt, info)
bool __platform_hkdf_sha256(
    platform_context_t ctx, // [IN] opaque platform context
    slice_ref_t ikm, // [IN] input key material
    slice_ref_t salt, // [IN] salt
    slice_ref_t info, // [IN] info
    slice_mut_t result // [IN/OUT] result.size sets length for hkdf, result.data is where the digest will be placed
) {
    size_t i;
    for (i = 0; i < result.size; i++) {
        result.data[i] = (uint8_t)(i & 0xFF);
    }
    return true;
}

// Calculate SH256 for the provided buffer
bool __platform_sha256(
    platform_context_t ctx, // [IN] opaque platform context
    slice_ref_t data, // [IN] data to hash
    digest_mut_t digest // [OUT] resulting digest
) {
    return false;
}

// Get device state
bool __platform_get_state(
    platform_context_t ctx, // [IN] opaque platform context
    digest_mut_t code_digest, // [OUT] "code digest" (GSCVD digest)
    uint32_t *sec_ver, // [OUT] security version (GSCVD version)
    uint32_t *aprov_status, // [OUT] APROV status
    digest_mut_t pcr0 // [OUT] PCR0 value
) {
    return false;
}

// Generate ECDSA P-256 key using HMAC-DRBG initialized by the seed
bool __platform_ecdsa_keygen_hmac_drbg(
    platform_context_t ctx, // [IN] opaque platform context
    digest_t seed, // [IN] key seed
    ecdsa_handle_t *key // [OUT] ECDSA key handle
) {
    return false;
}

// Generate ECDSA P-256 signature
bool __platform_ecdsa_p256_sign(
    platform_context_t ctx, // [IN] opaque platform context
    ecdsa_handle_t key, // [IN] ECDSA key handle
    slice_ref_t data, // [IN] data to sign
    ecdsa_sig_mut_t signature // [OUT] resulting signature: 64-bytes (R | S)
) {
    return false;
}

// Get ECDSA public key X, Y
bool __platform_get_pub_key(
    platform_context_t ctx, // [IN] opaque platform context
    ecdsa_handle_t key, // [IN] ECDSA key handle
    ecdsa_public_mut_t pub_key // [OUT] public key structure
) {
    return false;
}

// Check if APROV status allows making 'normal' boot mode decision
bool __platform_aprov_status_allows_normal(
    uint32_t aprov_status // [IN] APROV status
) {
    return true;
}

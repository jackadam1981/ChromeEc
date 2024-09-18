// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform.h"
#include <stdio.h>
#include <string.h>

// Perform HKDF-SHA256(ikm, salt, info)
bool __platform_hkdf_sha256(
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
    slice_ref_t data, // [IN] data to hash
    uint8_t digest[DIGEST_BYTES] // [OUT] resulting digest
) {
    return false;
}

// Get DICE config
bool __platform_get_dice_config(
    dice_config_t *cfg // [OUT] DICE config
) {
    return false;
}

// Generate ECDSA P-256 key using HMAC-DRBG initialized by the seed
bool __platform_ecdsa_p256_keygen_hmac_drbg(
    const uint8_t seed[DIGEST_BYTES], // [IN] key seed
    ecdsa_handle_t *key // [OUT] ECDSA key handle
) {
    return false;
}

// Generate ECDSA P-256 signature
bool __platform_ecdsa_p256_sign(
    ecdsa_handle_t key, // [IN] ECDSA key handle
    slice_ref_t data, // [IN] data to sign
    uint8_t signature[ECDSA_SIG_BYTES] // [OUT] resulting signature: 64-bytes (R | S)
) {
    return false;
}

// Get ECDSA public key X, Y
bool __platform_ecdsa_p256_get_pub_key(
    ecdsa_handle_t key, // [IN] ECDSA key handle
    ecdsa_public_t *pub_key // [OUT] public key structure
) {
    return false;
}

// Free ECDSA key handle
void __platform_ecdsa_p256_free(
    ecdsa_handle_t key // [IN] ECDSA key handle
) {

}

// Check if APROV status allows making 'normal' boot mode decision
bool __platform_aprov_status_allows_normal(
    uint32_t aprov_status // [IN] APROV status
) {
    return true;
}

// Print error string to log
void __platform_log_str(
    const char *str // [IN] string to print
) {
    puts(str);
}

// memcpy
void __platform_memcpy(void *dest, const void * src, size_t size) {
    memcpy(dest, src, size);
}

// memset
void __platform_memset(void *dest, uint8_t fill, size_t size) {
    memset(dest, fill, size);
}

// memcmp
int __platform_memcmp(const void *str1, const void *str2, size_t size) {
    return memcmp(str1, str2, size);
}


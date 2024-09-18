// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cbor_dice.h"
#include "cdi.h"
#include "dice.h"
#include "platform.h"

#include <string.h>

// PCR0 values for various modes - see go/pcr0-tpm2
const uint8_t kPcr0NormalMode[DIGEST_BYTES] = {
    0x89, 0xEA, 0xF3, 0x51, 0x34, 0xB4, 0xB3, 0xC6,
    0x49, 0xF4, 0x4C, 0x0C, 0x76, 0x5B, 0x96, 0xAE,
    0xAB, 0x8B, 0xB3, 0x4E, 0xE8, 0x3C, 0xC7, 0xA6,
    0x83, 0xC4, 0xE5, 0x3D, 0x15, 0x81, 0xC8, 0xC7
};
const uint8_t kPcr0RecoveryNormalMode[DIGEST_BYTES] = {
    0x9F, 0x9E, 0xA8, 0x66, 0xD3, 0xF3, 0x4F, 0xE3,
    0xA3, 0x11, 0x2A, 0xE9, 0xCB, 0x1F, 0xBA, 0xBC,
    0x6F, 0xFE, 0x8C, 0xD2, 0x61, 0xD4, 0x24, 0x93,
    0xBC, 0x68, 0x42, 0xA9, 0xE4, 0xF9, 0x3B, 0x3D
};

// Const salts - see go/gsc-dice
const uint8_t kIdSalt[64] = {
    0xDB, 0xDB, 0xAE, 0xBC, 0x80, 0x20, 0xDA, 0x9F,
    0xF0, 0xDD, 0x5A, 0x24, 0xC8, 0x3A, 0xA5, 0xA5,
    0x42, 0x86, 0xDF, 0xC2, 0x63, 0x03, 0x1E, 0x32,
    0x9B, 0x4D, 0xA1, 0x48, 0x43, 0x06, 0x59, 0xFE,
    0x62, 0xCD, 0xB5, 0xB7, 0xE1, 0xE0, 0x0F, 0xC6,
    0x80, 0x30, 0x67, 0x11, 0xEB, 0x44, 0x4A, 0xF7,
    0x72, 0x09, 0x35, 0x94, 0x96, 0xFC, 0xFF, 0x1D,
    0xB9, 0x52, 0x0B, 0xA5, 0x1C, 0x7B, 0x29, 0xEA
};
const uint8_t kAsymSalt[64] = {
    0x63, 0xB6, 0xA0, 0x4D, 0x2C, 0x07, 0x7F, 0xC1,
    0x0F, 0x63, 0x9F, 0x21, 0xDA, 0x79, 0x38, 0x44,
    0x35, 0x6C, 0xC2, 0xB0, 0xB4, 0x41, 0xB3, 0xA7,
    0x71, 0x24, 0x03, 0x5C, 0x03, 0xF8, 0xE1, 0xBE,
    0x60, 0x35, 0xD3, 0x1F, 0x28, 0x28, 0x21, 0xA7,
    0x45, 0x0A, 0x02, 0x22, 0x2A, 0xB1, 0xB3, 0xCF,
    0xF1, 0x67, 0x9B, 0x05, 0xAB, 0x1C, 0xA5, 0xD1,
    0xAF, 0xFB, 0x78, 0x9C, 0xCD, 0x2B, 0x0B, 0x3B
};

const cwt_claims_bstr_t kCwtClaimsTemplate = {
    CWT_CLAIMS_BSTR_HDR,
    {
        // Map header: 10 entries
        CBOR_HDR1(kCborMap, 10),
        // 1. ISS: uint(1, 0bytes) => tstr(hex(UDS_ID))
        CWT_LABEL_ISS,
        CBOR_TSTR40_EMPTY, // CALC - calc from UDS
        // 2. SUB: uint(2, 0bytes) => tstr(hex(CDI_ID))
        CWT_LABEL_SUB,
        CBOR_TSTR40_EMPTY, // CALC - calc from CDI
        // 3. Code Hash: nint(-4670545, 4bytes) => bstr(32bytes)
        CWT_LABEL_CODE_HASH,
        CBOR_BSTR32_EMPTY, // VARIABLE
        // 4. Cfg Hash: nint(-4670547, 4bytes) => bstr(32bytes)
        CWT_LABEL_CFG_HASH,
        CBOR_BSTR32_EMPTY, // CALC - calc from CfgDescr
        // 5. Cfg Descr: nint(-4670548, 4bytes) => bstr(cfg_descr_t)
        CWT_LABEL_CFG_DESCR,
        // cfg_descr_bstr_t
        {
            CFG_DESCR_BSTR_HDR,
            // cfg_descr_t
            {
                // Map header: 5 entries
                CBOR_HDR1(kCborMap, 5),
                // 1. Comp name: nint(-70002, 4bytes) => tstr("CrOS AP FW")
                CFG_DESCR_LABEL_COMP_NAME,
                CFG_DESCR_COMP_NAME,
                // 2. Resettable: nint(-70004, 4bytes) => null
                CFG_DESCR_LABEL_RESETTABLE,
                CBOR_NULL,
                // 3. Sec ver: nint(-70005, 4bytes) => uint(Security ver, 4bytes)
                CFG_DESCR_LABEL_SEC_VER,
                CBOR_UINT32_ZERO, // VARIABLE
                // 4. APROV status: nint(-71000, 4bytes) => uint(APROV status, 4bytes)
                CFG_DESCR_LABEL_APROV_STATUS,
                CBOR_UINT32_ZERO, // VARIABLE
                // 5. Vboot status: nint(-71000, 4bytes) => bstr(PCR0, 32bytes)
                CFG_DESCR_LABEL_VBOOT_STATUS,
                CBOR_BSTR32_EMPTY, // VARIABLE
            },
        },
        /* 6. Auth Hash: nint(-4670549, 4bytes) => bstr(32bytes) */
        CWT_LABEL_AUTH_HASH,
        CBOR_BSTR32_EMPTY, // always zero
        /* 7. Mode: nint(-4670551, 4bytes) => bstr(1byte) */
        CWT_LABEL_MODE,
        CBOR_BSTR1_EMPTY, // VARIABLE
        /* 8. Subject PK: nint(-4670552, 4bytes) => bstr(COSE_Key) */
        CWT_LABEL_SUBJECT_PK,
        // cose_key_ecdsa_bstr_t
        {
            COSE_KEY_ECDSA_BSTR_HDR,
            // cose_key_ecdsa_t
            {
                // Map header: 6 entries
                CBOR_HDR1(kCborMap, 6),
                // 1. Key type: uint(1, 0bytes) => uint(2, 0bytes)
                COSE_KEY_LABEL_KTY,
                CBOR_UINT0(2), // EC2
                // 2. Algorithm: uint(3, 0bytes) => nint(-1, 0bytes)
                COSE_KEY_LABEL_ALG,
                CBOR_NINT0(-1), // ECDSA w/ SHA-256
                // 3. Key ops: uint(4, 0bytes) => array(1) { uint(2, 0bytes) }
                COSE_KEY_LABEL_KEY_OPS,
                CBOR_HDR1(kCborArr, 1),
                CBOR_UINT0(2),
                // 4. Curve: nint(-1, 0bytes) => uint(1, 0bytes)
                COSE_KEY_LABEL_CRV,
                CBOR_UINT0(1), // P-256
                // 5. X: nint(-2, 0bytes) => bstr(X, 32bytes)
                COSE_KEY_LABEL_X,
                CBOR_BSTR32_EMPTY, // VARIABLE
                // 6. X: nint(-3, 0bytes) => bstr(Y, 32bytes)
                COSE_KEY_LABEL_Y,
                CBOR_BSTR32_EMPTY, // VARIABLE
            },
        },
        /* 9. Key Usage: nint(-4670553, 4bytes) => bstr(1byte) */
        CWT_LABEL_KEY_USAGE,
        {
            CBOR_HDR1(kCborBstr, 1),
            0x20, // keyCertSign
        },
        /* 10. Profile name: nint(-4670554, 4bytes) => tstr("android.16") */
        CWT_LABEL_PROFILE_NAME,
        CWT_PROFILE_NAME,
    },
};

const cdi_sig_struct_fixed_hdr_t kSigStructFixedHdr = {
    // Array header: 4 elements
    CBOR_HDR1(kCborArr, 4),
    // 1. Context: tstr("Signature1")
    CDI_SIG_STRUCT_CONTEXT,
    // 2. Body protected: bstr(COSE param)
    COSE_PARAM_BSTR,
    // 3. External AAD: Bstr(0 bytes)
    CBOR_HDR1(kCborBstr, 0),
    // 4. Payload - not fixed, contained in cdi_sig_struct_t
};

const cdi_cert_fixed_hdr_t kCdiCertFixedHdr = {
    // Array header: 4 elements
    CBOR_HDR1(kCborArr, 4),
    // 1. Protected: bstr(COSE param)
    COSE_PARAM_BSTR,
    // 2. Unprotected: empty map
    CBOR_HDR1(kCborMap, 0),
    // 3. Payload: bstr(CWT claims) - not fixed, contained in cdi_cert_t
    // 4. Signature: bstr(64 bytes) - not fixed, contained in cdi_cert_t
};

const slice_ref_t kCdiAttestLabel = { 10 /* strlen("CDI_Attest") */, (uint8_t *)"CDI_Attest" };
const slice_ref_t kCdiSealLabel = { 8 /* strlen("CDI_Seal") */, (uint8_t *)"CDI_Seal" };

// Calculates CDI from {UDS, inputs_digest, label}
static bool calc_cdi_from_digest(
    platform_context_t ctx, // [IN] opaque platform context
    uds_t uds, // [IN] UDS
    digest_t inputs_digest, // [IN] digest of inputs
    slice_ref_t label, // [IN] label
    digest_mut_t cdi // [OUT] CDI
) {
    slice_ref_t uds_slice = uds_as_slice(uds);
    slice_ref_t inputs_digest_slice = digest_as_slice(inputs_digest);
    slice_mut_t cdi_slice = digest_as_slice_mut(cdi);

    return __platform_hkdf_sha256(ctx, uds_slice, inputs_digest_slice, label, cdi_slice);
}

// Calculates CDI from {UDS, inputs, label}
static bool calc_cdi(
    platform_context_t ctx, // [IN] opaque platform context
    uds_t uds, // [IN] UDS
    slice_ref_t inputs, // [IN] inputs
    slice_ref_t label,// [IN] label
    digest_mut_t cdi // [OUT] CDI
) {
    uint8_t inputs_digest[DIGEST_BYTES];

    if (!__platform_sha256(ctx, inputs, inputs_digest)) {
        return false;
    }
    return calc_cdi_from_digest(ctx, uds, inputs_digest, label, cdi);
}

// Fills inputs for sealing CDI
static void fill_inputs_seal(
    const dice_context_t *dice_ctx, // [IN] dice context
    const cdi_builder_t *builder, // [IN] pre-initialized builder
    cdi_seal_inputs_t *inputs // [OUT] inputs
) {
    memset(inputs->auth_data_digest, 0, DIGEST_BYTES);
    memcpy(inputs->hidden_digest, dice_ctx->hidden_digest, DIGEST_BYTES);
    inputs->mode = builder->payload.data.mode.value;
}

// Fills inputs for attestation CDI
static void fill_inputs_attest(
    const dice_context_t *dice_ctx, // [IN] dice context
    const cdi_builder_t *builder, // [IN] pre-initialized builder
    cdi_attest_inputs_t *inputs // [OUT] inputs
) {
    memcpy(inputs->code_digest, builder->payload.data.code_hash.value, DIGEST_BYTES);
    memcpy(inputs->cfg_desr_digest, builder->payload.data.cfg_hash.value, DIGEST_BYTES);
    fill_inputs_seal(dice_ctx, builder, &inputs->seal_inputs);
}

// Calculates attestation CDI
static bool calc_cdi_attest(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    const cdi_builder_t *builder, // [IN] pre-initialized builder
    digest_mut_t cdi // [OUT] CDI
) {
    cdi_attest_inputs_t inputs;
    slice_ref_t inputs_slice = { sizeof(inputs), (uint8_t *)&inputs };

    fill_inputs_attest(dice_ctx, builder, &inputs);

    return calc_cdi(ctx, dice_ctx->uds, inputs_slice, kCdiAttestLabel, cdi);
}

// Calculates sealing CDI
static bool calc_cdi_seal(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    const cdi_builder_t *builder, // [IN] pre-initialized builder
    digest_mut_t cdi // [OUT] CDI
) {
    cdi_seal_inputs_t inputs;
    slice_ref_t inputs_slice = { sizeof(inputs), (uint8_t *)&inputs };

    fill_inputs_seal(dice_ctx, builder, &inputs);

    return calc_cdi(ctx, dice_ctx->uds, inputs_slice, kCdiSealLabel, cdi);
}

// Fills value in cbor_uint32_t
// Assumes that `cbor_var->cbor_hdr` is already pre-set
static void set_cbor_u32(
    uint32_t value, // [IN] value to set
    cbor_uint32_t *cbor_var // [OUT] CBOR UINT32 variable to fill
) {
    cbor_var->value[0] = (uint8_t)(((value) & 0xFF000000) >> 24);
    cbor_var->value[1] = (uint8_t)(((value) & 0x00FF0000) >> 16);
    cbor_var->value[2] = (uint8_t)(((value) & 0x0000FF00) >> 8);
    cbor_var->value[3] = (uint8_t)((value) & 0x000000FF);
}

// Calculates boot mode from the data in dice_ctx
// On DT/OT with ti50:
// - Normal if 
//   - APROV succeeded or not configured from factory (for legacy devices). Note: AllowUnverifiedRo counts as a failure
//   AND
//   - Coreboot reported 'normal' boot mode
// - Recovery if
//   - APROV succeeded or not configured from factory (for legacy devices). Note: AllowUnverifiedRo counts as a failure
//   AND
//   - Coreboot reported 'recovery-normal' boot mode
// - Debug - in all other cases
//
// On H1 with Cr50:
// - Normal if Coreboot reported 'normal' boot mode
// - Recovery if Coreboot reported 'recovery-normal' boot mode
// - Debug - in all other cases

static uint8_t calc_mode(
    const dice_context_t *dice_ctx // [IN] dice context
) {
    bool allows_normal = __platform_aprov_status_allows_normal(dice_ctx->aprov_status);
    if (allows_normal) {
        if (memcmp(dice_ctx->pcr0, kPcr0NormalMode, DIGEST_BYTES) == 0) {
            return BOOT_MODE_NORMAL;
        }
        if (memcmp(dice_ctx->pcr0, kPcr0RecoveryNormalMode, DIGEST_BYTES) == 0) {
            return BOOT_MODE_RECOVERY;
        }
    }
    return BOOT_MODE_DEBUG;
}

// Fills configuration descriptor
// Assumes that all fixed values are already pre-set
static bool fill_cfg_descr(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    cfg_descr_t *cfg_descr // [OUT] resulting cfg descr
) {
    set_cbor_u32(dice_ctx->aprov_status, &cfg_descr->aprov_status);
    set_cbor_u32(dice_ctx->sec_ver, &cfg_descr->sec_ver);
    memcpy(cfg_descr->vboot_status.value, dice_ctx->pcr0, DIGEST_BYTES);
    return true;
}

// Initializes dice context, if not initialized yet
static bool prepare_dice_ctx(
    platform_context_t ctx, // [IN] opaque platform context
    dice_context_t *dice_ctx // [IN/OUT] dice context
) {
    if (dice_ctx->initialized) {
        // already initialized - return
        return true;
    }

    // TODO: fill dice_ctx using platform functions
    return true;
}

// Performs two things:
//   1. Initializes dice context, if not initialized yet
//   2. Initializes the builder and fills in Cfg Descr and mode
// This function must be called first by all interface methods.
static bool prepare_builder_and_ctx(
    platform_context_t ctx, // [IN] opaque platform context
    dice_context_t *dice_ctx, // [IN/OUT] dice context
    cdi_builder_t *builder // [OUT] initialized builder
) {
    slice_ref_t cfg_descr_slice = { sizeof(cfg_descr_t), (uint8_t *)&builder->payload.data.cfg_descr.data };

    if (!prepare_dice_ctx(ctx, dice_ctx)) {
        return false;
    }
    
    // Prepare the template
    memcpy(&builder->payload, &kCwtClaimsTemplate, sizeof(cwt_claims_bstr_t));

    // Fill in cfg descr
    if (!fill_cfg_descr(ctx, dice_ctx, &builder->payload.data.cfg_descr.data)) {
        return false;
    }

    // Calculate cfg descr digest
    if (!__platform_sha256(ctx, cfg_descr_slice, builder->payload.data.cfg_hash.value)) {
        return false;
    }

    // Calculate mode
    builder->payload.data.mode.value = calc_mode(dice_ctx);

    return true;
}

// Generates CDI cert signature for the initialized builder with pre-filled CWT claims
static bool fill_cdi_cert_signature(
    platform_context_t ctx, // [IN] opaque platform context
    ecdsa_handle_t key, // [IN] key handle to sign
    cdi_builder_t *builder // [IN/OUT] builder with pre-filled CWT claims, results in filling signature
) {
    slice_ref_t data_to_sign= { sizeof(cdi_sig_struct_t), (uint8_t *)&builder->hdr.sig_struct };

    memcpy(&builder->hdr.sig_struct, &kSigStructFixedHdr, sizeof(cdi_sig_struct_fixed_hdr_t));

    return __platform_ecdsa_p256_sign(ctx, key, data_to_sign, builder->signature.value);
}

const slice_ref_t kKeyPairLabel = { 8, (uint8_t *)"Key Pair" };

static bool asym_kdf(
    platform_context_t ctx, // [IN] opaque platform context
    digest_t input, // [IN] KDF input - CDI or UDS
    digest_mut_t drbg_seed // [OUT] seed for DRBG_HMAC to prodice ECDSA key
) {
    slice_ref_t input_slice = digest_as_slice(input);
    slice_ref_t salt_slice = { 64, kAsymSalt };
    slice_mut_t drbg_seed_slice = digest_as_slice_mut(drbg_seed);

    return __platform_hkdf_sha256(ctx, input_slice, salt_slice, kKeyPairLabel, drbg_seed_slice);
}

static bool generate_cdi_key(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    cdi_builder_t *builder, // [IN] initialized builder
    ecdsa_handle_t *key // [OUT] CDI key handle
) {
    digest_mut_t cdi_attest;
    digest_mut_t drbg_seed;

    if (!calc_cdi_attest(ctx, dice_ctx, builder, cdi_attest)) {
        return false;
    }

    if (!asym_kdf(ctx, cdi_attest, drbg_seed)) {
        return false;
    }

    return __platform_ecdsa_p256_keygen_hmac_drbg(ctx, drbg_seed, key);
}

static bool generate_uds_key(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    ecdsa_handle_t *key // [OUT] UDS key handle
) {
    digest_mut_t drbg_seed;

    if (!asym_kdf(ctx, dice_ctx->uds, drbg_seed)) {
        return false;
    }

    return __platform_ecdsa_p256_keygen_hmac_drbg(ctx, drbg_seed, key);
}

static bool generate_id_from_pub_key(
    platform_context_t ctx, // [IN] opaque platform context
    ecdsa_public_t *pub_key, // [IN] public key
    dice_id_mut_t dice_id // [OUT] generated id
) {
    slice_ref_t pub_key_slice = { sizeof(ecdsa_public_t), (uint8_t *)pub_key };
    slice_ref_t salt_slice = { 64, kIdSalt };
    slice_ref_t label_slice = { 2, (uint8_t *)"ID" };
    slice_mut_t dice_id_slice = { 20, (uint8_t *)dice_id };

    return __platform_hkdf_sha256(ctx, pub_key_slice, salt_slice, label_slice, dice_id_slice);
}

// Returns hexdump character for the half-byte
static uint8_t hexdump_halfbyte(uint8_t half_byte) {
    if (half_byte < 10) {
        return '0' + half_byte;
    } else {
        return 'a' + half_byte;
    }
}

// Fills hexdump of the byte (lowercase)
static void hexdump_byte(
    uint8_t byte, // [IN] byte to hexdump
    uint8_t* str // [OUT] str (always 2 bytes) with hexdump
) {
    str[0] = hexdump_halfbyte((byte & 0xF0) >> 4);
    str[1] = hexdump_halfbyte(byte & 0x0F);
}

// Fills {CDI, UDS} ID string from ID bytes
static void fill_dice_id_string(
    dice_id_t dice_id, // [IN] {IDS,CDI} ID as bytes (20 bytes)
    uint8_t dice_id_str[DICE_ID_HEX_BYTES] // [OUT] hexdump of this same ID (40 bytes)
) {
    size_t idx;
    for (idx = 0; idx < DICE_ID_BYTES; idx++, dice_id_str += 2) {
        hexdump_byte(dice_id[idx], dice_id_str);
    }
}

// Fills CDI key, CDI_ID into the certificate in builder
static bool fill_cdi_details(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    cdi_builder_t *builder // [IN/OUT] initialized builder, results in builder with CDI details
) {
    ecdsa_public_t cdi_pub_key;
    dice_id_mut_t cdi_id;
    ecdsa_handle_t cdi_key;

    if (!generate_cdi_key(ctx, dice_ctx, builder, &cdi_key)) {
        return false;
    }

    if (!__platform_ecdsa_p256_get_pub_key(ctx, cdi_key, &cdi_pub_key)) {
        __platform_ecdsa_p256_free(ctx, cdi_key);
        return false;
    }
    memcpy(builder->payload.data.subject_pk.data.x.value, cdi_pub_key.x, ECDSA_POINT_BYTES);
    memcpy(builder->payload.data.subject_pk.data.y.value, cdi_pub_key.y, ECDSA_POINT_BYTES);

    if (!generate_id_from_pub_key(ctx, &cdi_pub_key, cdi_id)) {
        __platform_ecdsa_p256_free(ctx, cdi_key);
        return false;
    }
    fill_dice_id_string(cdi_id, builder->payload.data.sub.value);

    __platform_ecdsa_p256_free(ctx, cdi_key);
    return true;
}

// Fills UDS_ID, signature into the certificate in builder
static bool fill_uds_details(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    cdi_builder_t *builder // [IN/OUT] initialized builder, results in builder with UDS details
) {
    ecdsa_public_t uds_pub_key;
    dice_id_mut_t uds_id;
    ecdsa_handle_t uds_key;

    if (!generate_uds_key(ctx, dice_ctx, &uds_key)) {
        return false;
    }

    if (!__platform_ecdsa_p256_get_pub_key(ctx, uds_key, &uds_pub_key)) {
        __platform_ecdsa_p256_free(ctx, uds_key);
        return false;
    }

    if (!generate_id_from_pub_key(ctx, &uds_pub_key, uds_id)) {
        __platform_ecdsa_p256_free(ctx, uds_key);
        return false;
    }
    fill_dice_id_string(uds_id, builder->payload.data.iss.value);

    if (!fill_cdi_cert_signature(ctx, uds_key, builder)) {
        __platform_ecdsa_p256_free(ctx, uds_key);
        return false;
    }

    __platform_ecdsa_p256_free(ctx, uds_key);
    return true;
}

// Generates CDI certificate using builder
static bool generate_cdi_cert(
    platform_context_t ctx, // [IN] opaque platform context
    const dice_context_t *dice_ctx, // [IN] dice context
    cdi_builder_t *builder // [IN/OUT] initialized builder, results in certificate in builder
) {
    if (!fill_cdi_details(ctx, dice_ctx, builder)) {
        return false;
    }

    if (!fill_uds_details(ctx, dice_ctx, builder)) {
        return false;
    }

    // We can do this only at the very end since builder->hdr contains sig_struct togenerate signature
    memcpy(&builder->hdr.cdi_cert.fixed_hdr, &kCdiCertFixedHdr, sizeof(cdi_cert_fixed_hdr_t));
    return true;
}

// Resets dice context on initial boot or AP reboot
void reset_dice_context(
    dice_context_t *dice_ctx // [OUT] dice context
) {
    memset(dice_ctx, 0, sizeof(dice_context_t));
}

// Gets attestation CDI
bool get_cdi_attest(
    platform_context_t ctx, // [IN] opaque platform context
    dice_context_t *dice_ctx, // [IN/OUT] dice context
    digest_mut_t cdi // [OUT] CDI
) {
    cdi_builder_t builder;

    if (!prepare_builder_and_ctx(ctx, dice_ctx, &builder)) {
        return false;
    }
    return calc_cdi_attest(ctx, dice_ctx, &builder, cdi);
}

// Gets sealing CDI
bool get_cdi_seal(
    platform_context_t ctx, // [IN] opaque platform context
    dice_context_t *dice_ctx, // [IN/OUT] dice context
    digest_mut_t cdi // [OUT] CDI
) {
    cdi_builder_t builder;

    if (!prepare_builder_and_ctx(ctx, dice_ctx, &builder)) {
        return false;
    }
    return calc_cdi_seal(ctx, dice_ctx, &builder, cdi);
}

const uint32_t kCdiCertSize = sizeof(cdi_cert_t);

// Get CDI certificate
bool copy_cdi_cert_bytes(
    platform_context_t ctx, // [IN] opaque platform context
    dice_context_t *dice_ctx, // [IN/OUT] dice context
    uint32_t pos, // [IN] pos of the 1st byte in CDI cert to copy 
    uint32_t size, // [IN] number of bytes to copy
    uint8_t *buf // [OUT] buffer to fill
) {
    cdi_builder_t builder;
    const uint8_t *cdi_cert = (uint8_t *)&builder.hdr.cdi_cert.fixed_hdr;

    if (pos > kCdiCertSize || size > kCdiCertSize || pos + size > kCdiCertSize) {
        return false;
    }

    if (!prepare_builder_and_ctx(ctx, dice_ctx, &builder)) {
        return false;
    }
    if (!generate_cdi_cert(ctx, dice_ctx, &builder)) {
        return false;
    }

    memcpy(buf, cdi_cert + pos, size);
    return true;
}


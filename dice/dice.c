// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cbor_dice.h"
#include "cdi.h"
#include "dice.h"
#include "platform.h"

#define RET_IF_FAIL_RETVAL(f, e) if (!f) { return e; }
#define RET_IF_FAIL_RETVAL_S(f, e, s) if (!f) { __platform_log_str(s); return e; }

#define RET_IF_FAIL(f) RET_IF_FAIL_RETVAL(f, false)
#define RET_IF_FAIL_S(f, s) RET_IF_FAIL_RETVAL_S(f, false, s)

////////////////////////////////////////////////////////////////////////////////
// Common structure to build Sig_structure or DICE Handover structure

// Combined headers before cert payload (CWT claims) in DICE handover
typedef struct {
    dice_cert_chain_hdr_t cert_chain;
    cdi_cert_hdr_t cert;
} combined_hdr_t;

typedef union {
    uint8_t sig_struct[sizeof(combined_hdr_t)];
    combined_hdr_t dice_handover;
} header_options_t;

// We need the following to be able to fill CDIs in dice_handover_t::hdr
// before we temporarily use dice_handover_t::options.sig_struct to calc signature.
_Static_assert(sizeof(combined_hdr_t) >= sizeof(cdi_sig_struct_hdr_t));

typedef struct {
    dice_handover_hdr_t hdr;
    header_options_t options;
    cwt_claims_bstr_t payload;
    cbor_bstr64_t signature;
} dice_handover_t;

////////////////////////////////////////////////////////////////////////////////
// Context to pass between functions that build the DICE handover structure

typedef struct {
    dice_handover_t output;
    dice_config_t cfg;
} dice_ctx_t;

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
                // Map header: 6 entries
                CBOR_HDR1(kCborMap, 6),
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
                // 6. AP FW version: nint(-71002, 4bytes) => bstr(PCR10, 32bytes)
                CFG_DESCR_LABEL_AP_FW_VERSION,
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

const dice_handover_hdr_t kDiceHandoverHdrTemplate = {
    // Map header: 3 elements
    CBOR_HDR1(kCborMap, 3),
    // 1. CDI_Attest: uint(1, 0bytes) => bstr(32bytes)
    DICE_HANDOVER_LABEL_CDI_ATTEST,
    CBOR_BSTR32_EMPTY, // CALC - CDI_attest
    // 2. CDI_Seal: uint(2, 0bytes) => bstr(32bytes)
    DICE_HANDOVER_LABEL_CDI_SEAL,
    CBOR_BSTR32_EMPTY, // CALC - CDI_seal
    // 3. DICE chain: uint(3, 0bytes) => DICE cert chain
    DICE_HANDOVER_LABEL_DICE_CHAIN
    // DICE cert chain is not included
};

const cdi_sig_struct_hdr_t kSigStructFixedHdr = {
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

const combined_hdr_t kCombinedHdrTemplate = {
    // dice_cert_chain_hdr_t cert_chain
    {
        // Array header: 2 elements
        CBOR_HDR1(kCborArr, 2),
        // 1. UDS pub key: COSE_Key
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
        // 2. CDI DICE cert: - not included,
        // consists of hdr=cdi_cert_hdr_t, payload=cwt_claims_bstr_t, sig=cbor_bstr64_t
    },
    // cdi_cert_hdr_t cert
    {
        // Array header: 4 elements
        CBOR_HDR1(kCborArr, 4),
        // 1. Protected: bstr(COSE param)
        COSE_PARAM_BSTR,
        // 2. Unprotected: empty map
        CBOR_HDR1(kCborMap, 0),
        // 3. Payload: bstr(CWT claims) - not fixed, contained in cdi_cert_t
        // 4. Signature: bstr(64 bytes) - not fixed, contained in cdi_cert_t
    }
};

const slice_ref_t kCdiAttestLabel = { 10 /* strlen("CDI_Attest") */, (uint8_t *)"CDI_Attest" };
const slice_ref_t kCdiSealLabel = { 8 /* strlen("CDI_Seal") */, (uint8_t *)"CDI_Seal" };
const slice_ref_t kIdSaltSlice = { 64, (const uint8_t *)kIdSalt };
const slice_ref_t kAsymSaltSlice = { 64, (const uint8_t *)kAsymSalt };
const slice_ref_t kIdLabel = { 2, (uint8_t *)"ID" };

// Calculates CDI from {UDS, inputs_digest, label}
static bool calc_cdi_from_digest(
    const uint8_t uds[DIGEST_BYTES], // [IN] UDS
    const uint8_t inputs_digest[DIGEST_BYTES], // [IN] digest of inputs
    slice_ref_t label, // [IN] label
    uint8_t cdi[DIGEST_BYTES] // [OUT] CDI
) {
    slice_ref_t uds_slice = digest_as_slice(uds);
    slice_ref_t inputs_digest_slice = digest_as_slice(inputs_digest);
    slice_mut_t cdi_slice = digest_as_slice_mut(cdi);

    return __platform_hkdf_sha256(uds_slice, inputs_digest_slice, label, cdi_slice);
}

// Calculates CDI from {UDS, inputs, label}
static bool calc_cdi(
    const uint8_t uds[DIGEST_BYTES], // [IN] UDS
    slice_ref_t inputs, // [IN] inputs
    slice_ref_t label,// [IN] label
    uint8_t cdi[DIGEST_BYTES] // [OUT] CDI
) {
    uint8_t inputs_digest[DIGEST_BYTES];

    RET_IF_FAIL_S(__platform_sha256(inputs, inputs_digest), "Failed to hash inputs");
    return calc_cdi_from_digest(uds, inputs_digest, label, cdi);
}

// Fills inputs for sealing CDI.
// Assumes that ctx->cfg and CfgDescr in ctx->output are already filled.
static void fill_inputs_seal(
    const dice_ctx_t *ctx, // [IN] dice context
    cdi_seal_inputs_t *inputs // [OUT] inputs
) {
    __platform_memset(inputs->auth_data_digest, 0, DIGEST_BYTES);
    __platform_memcpy(inputs->hidden_digest, ctx->cfg.hidden_digest, DIGEST_BYTES);
    inputs->mode = ctx->output.payload.data.mode.value;
}

// Fills inputs for attestation CDI
// Assumes that ctx->cfg and CfgDescr in ctx->output are already filled.
static void fill_inputs_attest(
    const dice_ctx_t *ctx, // [IN] dice context
    cdi_attest_inputs_t *inputs // [OUT] inputs
) {
    __platform_memcpy(inputs->code_digest, ctx->output.payload.data.code_hash.value, DIGEST_BYTES);
    __platform_memcpy(inputs->cfg_desr_digest, ctx->output.payload.data.cfg_hash.value, DIGEST_BYTES);
    fill_inputs_seal(ctx, &inputs->seal_inputs);
}

// Calculates attestation CDI.
// Assumes that ctx->cfg and CfgDescr in ctx->output are already filled.
static bool calc_cdi_attest(
    const dice_ctx_t *ctx, // [IN] dice context
    uint8_t cdi[DIGEST_BYTES] // [OUT] CDI
) {
    cdi_attest_inputs_t inputs;
    slice_ref_t inputs_slice = { sizeof(inputs), (uint8_t *)&inputs };

    fill_inputs_attest(ctx, &inputs);
    return calc_cdi(ctx->cfg.uds, inputs_slice, kCdiAttestLabel, cdi);
}

// Calculates sealing CDI.
// Assumes that ctx->cfg and CfgDescr in ctx->output are already filled.
static bool calc_cdi_seal(
    const dice_ctx_t *ctx, // [IN] dice context
    uint8_t cdi[DIGEST_BYTES] // [OUT] CDI
) {
    cdi_seal_inputs_t inputs;
    slice_ref_t inputs_slice = { sizeof(inputs), (uint8_t *)&inputs };

    fill_inputs_seal(ctx, &inputs);
    return calc_cdi(ctx->cfg.uds, inputs_slice, kCdiSealLabel, cdi);
}

// Calculates boot mode from the data in ctx->cfg
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
    const dice_ctx_t *ctx // [IN] dice context
) {
    bool allows_normal = __platform_aprov_status_allows_normal(ctx->cfg.aprov_status);
    if (allows_normal) {
        if (__platform_memcmp(ctx->cfg.pcr0, kPcr0NormalMode, DIGEST_BYTES) == 0) {
            return BOOT_MODE_NORMAL;
        }
        if (__platform_memcmp(ctx->cfg.pcr0, kPcr0RecoveryNormalMode, DIGEST_BYTES) == 0) {
            return BOOT_MODE_RECOVERY;
        }
    }
    return BOOT_MODE_DEBUG;
}

// Generates CDI cert signature for the initialized builder with pre-filled CWT claims.
static bool fill_cdi_cert_signature(
    dice_ctx_t *ctx, // [IN/OUT] dice context
    ecdsa_handle_t key // [IN] key handle to sign
) {
    uint8_t * sig_struct = ((uint8_t *)&ctx->output.payload) - sizeof(cdi_sig_struct_hdr_t);
    slice_ref_t data_to_sign = { CDI_SIG_STRUCT_LEN, sig_struct };

    __platform_memcpy(sig_struct, &kSigStructFixedHdr, sizeof(cdi_sig_struct_hdr_t));
    return __platform_ecdsa_p256_sign(key, data_to_sign, ctx->output.signature.value);
}

const slice_ref_t kKeyPairLabel = { 8, (uint8_t *)"Key Pair" };

// Generates key from UDS or CDI_Attest value.
static bool generate_key(
    const uint8_t input[DIGEST_BYTES], // [IN] CDI_attest or UDS value
    ecdsa_handle_t *key // [OUT] key handle
) {
    uint8_t drbg_seed[DIGEST_BYTES];
    slice_ref_t input_slice = digest_as_slice(input);
    slice_mut_t drbg_seed_slice = digest_as_slice_mut(drbg_seed);

    RET_IF_FAIL_S(__platform_hkdf_sha256(input_slice, kAsymSaltSlice, kKeyPairLabel, drbg_seed_slice), "ASYM_KDF failed");
    return __platform_ecdsa_p256_keygen_hmac_drbg(drbg_seed, key);
}

// Generates {UDS, CDI}_ID from {UDS, CDI} public key.
static bool generate_id_from_pub_key(
    const ecdsa_public_t *pub_key, // [IN] public key
    uint8_t dice_id[DICE_ID_BYTES] // [OUT] generated id
) {
    slice_ref_t pub_key_slice = { sizeof(ecdsa_public_t), (const uint8_t *)pub_key };
    slice_mut_t dice_id_slice = { 20, (uint8_t *)dice_id };

    return __platform_hkdf_sha256(pub_key_slice, kIdSaltSlice, kIdLabel, dice_id_slice);
}

// Returns hexdump character for the half-byte.
static uint8_t hexdump_halfbyte(uint8_t half_byte) {
    if (half_byte < 10) {
        return '0' + half_byte;
    } else {
        return 'a' + half_byte;
    }
}

// Fills hexdump of the byte (lowercase).
static void hexdump_byte(
    uint8_t byte, // [IN] byte to hexdump
    uint8_t* str // [OUT] str (always 2 bytes) with hexdump
) {
    str[0] = hexdump_halfbyte((byte & 0xF0) >> 4);
    str[1] = hexdump_halfbyte(byte & 0x0F);
}

// Fills {CDI, UDS} ID string from ID bytes.
static void fill_dice_id_string(
    const uint8_t dice_id[DICE_ID_BYTES], // [IN] {IDS,CDI} ID as bytes (20 bytes)
    uint8_t dice_id_str[DICE_ID_HEX_BYTES] // [OUT] hexdump of this same ID (40 bytes)
) {
    size_t idx;
    for (idx = 0; idx < DICE_ID_BYTES; idx++, dice_id_str += 2) {
        hexdump_byte(dice_id[idx], dice_id_str);
    }
}

// Fills COSE_Key structure from pubkey.
// Assumes that all fields in COSE_Key except for X, y are already filled from template.
static void fill_cose_pubkey(
    const ecdsa_public_t *pub_key, // [IN] pub key
    cose_key_ecdsa_t *cose_key // [IN/OUT] COSE key structure
) {
    __platform_memcpy(cose_key->x.value, pub_key->x, ECDSA_POINT_BYTES);
    __platform_memcpy(cose_key->y.value, pub_key->y, ECDSA_POINT_BYTES);
}

// Fills CDI_attest pubkey, CDI_ID in the CDI certificate using generated CDI_attest key.
static bool fill_cdi_details_with_key(
    dice_ctx_t *ctx, // [IN/OUT] dice context
    ecdsa_handle_t cdi_key // [IN] CDI_attest key handle
) {
    ecdsa_public_t cdi_pub_key;
    uint8_t cdi_id[DICE_ID_BYTES];
    cwt_claims_t *cwt_claims = &ctx->output.payload.data;

    RET_IF_FAIL_S(__platform_ecdsa_p256_get_pub_key(cdi_key, &cdi_pub_key), "Failed to get CDI pubkey");
    fill_cose_pubkey(&cdi_pub_key, &cwt_claims->subject_pk.data);
    RET_IF_FAIL_S(generate_id_from_pub_key(&cdi_pub_key, cdi_id), "Failed to generate CDI_ID");
    fill_dice_id_string(cdi_id, cwt_claims->sub.value); // SUB = hex(CDI_ID)

    return true;
}

// Fills CDI_attest pubkey, CDI_ID in the CDI certificate.
// Assumes that ctx->cfg and CfgDescr in ctx->output are already filled.
static bool fill_cdi_details(
    dice_ctx_t *ctx // [IN/OUT] dice context
) {
    ecdsa_handle_t cdi_key;
    bool result;

    __platform_memcpy(&ctx->output.hdr, &kDiceHandoverHdrTemplate, sizeof(dice_handover_hdr_t));
    RET_IF_FAIL_S(calc_cdi_attest(ctx, ctx->output.hdr.cdi_attest.value), "Failed to calc CDI_attest");
    RET_IF_FAIL_S(calc_cdi_seal(ctx, ctx->output.hdr.cdi_seal.value), "Failed to calc CDI_seal");

    RET_IF_FAIL_S(generate_key(ctx->output.hdr.cdi_attest.value, &cdi_key), "Failed to generate CDI key");
    result = fill_cdi_details_with_key(ctx, cdi_key);
    __platform_ecdsa_p256_free(cdi_key);

    return result;
}

// Fills UDS_ID, signature into the certificate in builder using generated UDS key.
// Assumes that all other fields of CDI certificate were filled already.
static bool fill_uds_details_with_key(
    dice_ctx_t *ctx, // [IN/OUT] dice context
    ecdsa_handle_t uds_key // [IN] UDS key handle
) {
    ecdsa_public_t uds_pub_key;
    uint8_t uds_id[DICE_ID_BYTES];
    cwt_claims_t *cwt_claims = &ctx->output.payload.data;

    RET_IF_FAIL_S(__platform_ecdsa_p256_get_pub_key(uds_key, &uds_pub_key), "Failed to get UDS pubkey");
    RET_IF_FAIL_S(generate_id_from_pub_key(&uds_pub_key, uds_id), "Failed to generate UDS_ID");
    fill_dice_id_string(uds_id, cwt_claims->iss.value); // ISS = hex(UDS_ID)
    RET_IF_FAIL_S(fill_cdi_cert_signature(ctx, uds_key), "Failed to sign CDI cert");

    // We can do the rest only after we generated the signature because signature generation
    // uses ctx->output.hdr temporarily to build Sig_struct for signing.
    __platform_memcpy(&ctx->output.options.dice_handover, &kCombinedHdrTemplate, sizeof(combined_hdr_t));
    fill_cose_pubkey(&uds_pub_key, &ctx->output.options.dice_handover.cert_chain.uds_pub_key);

    return true;
}

// Fills UDS_ID, signature into the certificate in builder.
// Assumes that all other fields of CDI certificate were filled already.
static bool fill_uds_details(
    dice_ctx_t *ctx // [IN/OUT] dice context
) {
    ecdsa_handle_t uds_key;
    bool result;

    RET_IF_FAIL_S(generate_key(ctx->cfg.uds, &uds_key), "Failed to generate UDS key");
    result = fill_uds_details_with_key(ctx, uds_key);
    __platform_ecdsa_p256_free(uds_key);

    return result;
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

// Fills CfgDescr, CfgDescr digest and boot mode in CDI certificate
static bool fill_config_details(
    dice_ctx_t *ctx // [IN/OUT] dice context
) {
    cwt_claims_bstr_t *payload = &ctx->output.payload;
    cwt_claims_t *cwt_claims = &payload->data;
    cfg_descr_t *cfg_descr = &payload->data.cfg_descr.data;
    slice_ref_t cfg_descr_slice = { sizeof(cfg_descr_t), (uint8_t *)cfg_descr };

    // Copy fixed data from the template
    __platform_memcpy(payload, &kCwtClaimsTemplate, sizeof(cwt_claims_bstr_t));

    // Fill Cfg Descriptor variables based on ctx->cfg
    set_cbor_u32(ctx->cfg.aprov_status, &cfg_descr->aprov_status);
    set_cbor_u32(ctx->cfg.sec_ver, &cfg_descr->sec_ver);
    __platform_memcpy(cfg_descr->vboot_status.value, ctx->cfg.pcr0, DIGEST_BYTES);
    __platform_memcpy(cfg_descr->ap_fw_version.value, ctx->cfg.pcr10, DIGEST_BYTES);

    // Calculate Cfg Descriptor digest
    RET_IF_FAIL_S(__platform_sha256(cfg_descr_slice, cwt_claims->cfg_hash.value), "Failed to calc CfgDescr digest");

    // Calculate boot mode
    cwt_claims->mode.value = calc_mode(ctx);

    return true;
}

// Fills DICE handover structure in dice_ctx_t
// Assumes ctx.cfg is already filled
static bool generate_dice_handover(
    dice_ctx_t *ctx // [IN/OUT] dice context
) {
    // 1. Fill device configuration details in CDI certificate: CfgDescr and its digest, boot mode
    RET_IF_FAIL(fill_config_details(ctx));

    // 2. Fill CDI details in CDI certificate (CDI pubkey, CDI_ID) and DICE handover (CDIs)
    // Relies on config details to be filled already
    RET_IF_FAIL(fill_cdi_details(ctx));

    // 3. Fill UDS details in CDI certificate (UDS_ID, signature by UDS key) and DICE chain (UDS pubkey)
    // Relies on the rest of CDI certificate to be filled already.
    RET_IF_FAIL(fill_uds_details(ctx));

    return true;
}

// Get (part of) DICE handover structure: [offset .. offset + size)
size_t get_dice_handover_bytes(
    uint8_t *dest, // [OUT] destination buffer to fill
    size_t offset, // [IN] starting offset in the DICE handover structure
    size_t size // [IN] size of the DICE handover structure to copy
) {
    dice_ctx_t ctx;
    uint8_t *src = (uint8_t *)&ctx.output;

    if (size == 0 || offset >= sizeof(dice_handover_t)) {
        return 0;
    }
    if (size > sizeof(dice_handover_t) - offset) {
        size = sizeof(dice_handover_t) - offset;
    }
 
    RET_IF_FAIL_RETVAL_S(__platform_get_dice_config(&ctx.cfg), 0, "Failed to get DICE config");
    RET_IF_FAIL_RETVAL(generate_dice_handover(&ctx), 0);

    __platform_memcpy(dest, src + offset, size);
    return size;
}
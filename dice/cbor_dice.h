// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __GSC_UTILS_DICE_CBOR_DICE_H
#define __GSC_UTILS_DICE_CBOR_DICE_H

#include "cbor_basic.h"
#include "dice_types.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Boot mode decisions

// Boot mode == "Not configured" is not allowed
#define BOOT_MODE_NORMAL    1
#define BOOT_MODE_DEBUG     2
#define BOOT_MODE_RECOVERY  3

////////////////////////////////////////////////////////////////////////////////
// Configuration descriptor - see go/al-dice
#define CFG_DESCR_LABEL_COMP_NAME       CBOR_NINT32(-70002)
#define CFG_DESCR_LABEL_RESETTABLE      CBOR_NINT32(-70004)
#define CFG_DESCR_LABEL_SEC_VER         CBOR_NINT32(-70005)
#define CFG_DESCR_LABEL_APROV_STATUS    CBOR_NINT32(-71000)
#define CFG_DESCR_LABEL_VBOOT_STATUS    CBOR_NINT32(-71001)

#define CFG_DESCR_COMP_NAME_VALUE_LEN 10 /* "CrOS AP FW" */
#define CFG_DESCR_COMP_NAME_LEN (1 + CFG_DESCR_COMP_NAME_VALUE_LEN)
#define CFG_DESCR_COMP_NAME { \
    CBOR_HDR1(kCborBstr, CFG_DESCR_COMP_NAME_VALUE_LEN), \
    'C', 'r', 'O', 'S', ' ', 'A', 'P', ' ', 'F', 'W' \
}

typedef struct {
    // Map header: 5 entries
    uint8_t map_hdr;
    // 1. Comp name: nint(-70002, 4bytes) => tstr("CrOS AP FW")
    uint8_t comp_name_label[CBOR_NINT32_LEN];
    uint8_t comp_name[CFG_DESCR_COMP_NAME_LEN];
    // 2. Resettable: nint(-70004, 4bytes) => null
    uint8_t resettable_label[CBOR_NINT32_LEN];
    uint8_t resettable;
    // 3. Sec ver: nint(-70005, 4bytes) => uint(Security ver, 4bytes)
    uint8_t sec_ver_label[CBOR_NINT32_LEN];
    cbor_uint32_t sec_ver;
    // 4. APROV status: nint(-71000, 4bytes) => uint(APROV status, 4bytes)
    uint8_t aprov_status_label[CBOR_NINT32_LEN];
    cbor_uint32_t aprov_status;
    // 5. Vboot status: nint(-71000, 4bytes) => bstr(PCR0, 32bytes)
    uint8_t vboot_status_label[CBOR_NINT32_LEN];
    cbor_bstr32_t vboot_status;
} cfg_descr_t;

#define CFG_DESCR_LEN sizeof(cfg_descr_t)
typedef struct {
    uint8_t bstr_hdr[2]; // bstr(sizeof(cfg_descr_t), 1byte)
    cfg_descr_t data;
} cfg_descr_bstr_t;
#define CFG_DESCR_BSTR_HDR CBOR_BSTR_HDR8(CFG_DESCR_LEN)

////////////////////////////////////////////////////////////////////////////////
// COSE keys - see go/al-dice

#define COSE_KEY_LABEL_KTY          CBOR_UINT0(1)
#define COSE_KEY_LABEL_ALG          CBOR_UINT0(3)
#define COSE_KEY_LABEL_KEY_OPS      CBOR_UINT0(4)
#define COSE_KEY_LABEL_CRV          CBOR_NINT0(-1)
#define COSE_KEY_LABEL_X            CBOR_NINT0(-2)
#define COSE_KEY_LABEL_Y            CBOR_NINT0(-3)

// Configuration descriptor per go/al-dice
typedef struct {
    // Map header: 6 entries
    uint8_t map_hdr;
    // 1. Key type: uint(1, 0bytes) => uint(2, 0bytes)
    uint8_t kty_label;
    uint8_t kty;
    // 2. Algorithm: uint(3, 0bytes) => nint(-1, 0bytes)
    uint8_t alg_label;
    uint8_t alg;
    // 3. Key ops: uint(4, 0bytes) => array(1) { uint(2, 0bytes) }
    uint8_t key_ops_label;
    uint8_t key_ops_array_hdr;
    uint8_t key_ops;
    // 4. Curve: nint(-1, 0bytes) => uint(1, 0bytes)
    uint8_t crv_label;
    uint8_t crv;
    // 5. X: nint(-2, 0bytes) => bstr(X, 32bytes)
    uint8_t x_label;
    cbor_bstr32_t x;
    // 6. X: nint(-3, 0bytes) => bstr(Y, 32bytes)
    uint8_t y_label;
    cbor_bstr32_t y;
} cose_key_ecdsa_t;

#define COSE_KEY_ECDSA_LEN sizeof(cose_key_ecdsa_t)
typedef struct {
    uint8_t bstr_hdr[2]; // bstr(sizeof(cose_key_ecdsa_t), 1byte)
    cose_key_ecdsa_t data;
} cose_key_ecdsa_bstr_t;
#define COSE_KEY_ECDSA_BSTR_HDR CBOR_BSTR_HDR8(COSE_KEY_ECDSA_LEN)

////////////////////////////////////////////////////////////////////////////////
// CWT claims - see go/al-dice

// Size of TSTR containing an {UDS,CDI}_ID: (24 =< DICE_ID_HEX_LEN < 255) => 1 byte size encoding
#define DICE_ID_TSTR_LEN (2 + DICE_ID_HEX_BYTES)

#define CWT_LABEL_ISS               CBOR_UINT0(1)
#define CWT_LABEL_SUB               CBOR_UINT0(2)
#define CWT_LABEL_CODE_HASH         CBOR_NINT32(-4670545)
#define CWT_LABEL_CFG_HASH          CBOR_NINT32(-4670547)
#define CWT_LABEL_CFG_DESCR         CBOR_NINT32(-4670548)
#define CWT_LABEL_AUTH_HASH         CBOR_NINT32(-4670549)
#define CWT_LABEL_MODE              CBOR_NINT32(-4670551)
#define CWT_LABEL_SUBJECT_PK        CBOR_NINT32(-4670552)
#define CWT_LABEL_KEY_USAGE         CBOR_NINT32(-4670553)
#define CWT_LABEL_PROFILE_NAME      CBOR_NINT32(-4670554)

#define CWT_PROFILE_NAME_VALUE_LEN 10 /* "android.16" */
#define CWT_PROFILE_NAME_LEN (1 + CWT_PROFILE_NAME_VALUE_LEN)
#define CWT_PROFILE_NAME { \
    CBOR_HDR1(kCborTstr, CWT_PROFILE_NAME_VALUE_LEN), \
    'a', 'n', 'd', 'r', 'o', 'i', 'd', '.', '1', '6' \
}

typedef struct {
    // Map header: 10 entries
    uint8_t map_hdr;
    // 1. ISS: uint(1, 0bytes) => tstr(hex(UDS_ID))
    uint8_t iss_label;
    cbor_tstr40_t iss;
    // 2. SUB: uint(2, 0bytes) => tstr(hex(CDI_ID))
    uint8_t sub_label;
    cbor_tstr40_t sub;
    // 3. Code Hash: nint(-4670545, 4bytes) => bstr(32bytes)
    uint8_t code_hash_label[CBOR_NINT32_LEN];
    cbor_bstr32_t code_hash;
    // 4. Cfg Hash: nint(-4670547, 4bytes) => bstr(32bytes)
    uint8_t cfg_hash_label[CBOR_NINT32_LEN];
    cbor_bstr32_t cfg_hash;
    // 5. Cfg Descr: nint(-4670548, 4bytes) => bstr(cfg_descr_t)
    uint8_t cfg_descr_label[CBOR_NINT32_LEN];
    cfg_descr_bstr_t cfg_descr;
    // 6. Auth Hash: nint(-4670549, 4bytes) => bstr(32bytes)
    uint8_t auth_hash_label[CBOR_NINT32_LEN];
    cbor_bstr32_t auth_hash;
    // 7. Mode: nint(-4670551, 4bytes) => bstr(1byte)
    uint8_t mode_label[CBOR_NINT32_LEN];
    cbor_bstr1_t mode;
    // 8. Subject PK: nint(-4670552, 4bytes) => bstr(COSE_Key)
    uint8_t subject_pk_label[CBOR_NINT32_LEN];
    cose_key_ecdsa_bstr_t subject_pk;
    // 9. Key Usage: nint(-4670553, 4bytes) => bstr(1byte)
    uint8_t key_usage_label[CBOR_NINT32_LEN];
    cbor_bstr1_t key_usage;
    // 10. Profile name: nint(-4670554, 4bytes) => tstr("android.16")
    uint8_t profile_name_label[CBOR_NINT32_LEN];
    uint8_t profile_name[CWT_PROFILE_NAME_LEN];
} cwt_claims_t;

#define CWT_CLAIMS_LEN sizeof(cwt_claims_t)
typedef struct {
    uint8_t bstr_hdr[3]; // bstr(sizeof(cose_key_ecdsa_t), 2bytes)
    cwt_claims_t data;
} cwt_claims_bstr_t;
#define CWT_CLAIMS_BSTR_HDR CBOR_BSTR_HDR16(CWT_CLAIMS_LEN)

////////////////////////////////////////////////////////////////////////////////
// Protected COSE header parameters - see go/al-dice

#define COSE_PARAM_LABEL_ALG        CBOR_UINT0(1)
typedef struct {
    // BSTR of size 3 - see the rest of the struct
    uint8_t bstr_hdr;
    // Map header: 1 element
    uint8_t map_hdr;
    // 1. Alg: uint(1, 0bytes) => nint(-7, 0bytes)
    uint8_t alg_label;
    uint8_t alg;
} cose_param_bstr_t;

#define COSE_PARAM_BSTR { \
    /* BSTR of size 3 - see the rest of the struct */ \
    CBOR_HDR1(kCborBstr, 3), \
    /* Map header: 1 element */ \
    CBOR_HDR1(kCborMap, 1), \
    /* 1. Alg: uint(1, 0bytes) => nint(-7, 0bytes) */ \
    COSE_PARAM_LABEL_ALG, \
    CBOR_NINT0(-7) /* ECDSA w/ SHA-256 */ \
}

////////////////////////////////////////////////////////////////////////////////
// Sig structure for CDI certificate - see go/al-dice

#define CDI_SIG_STRUCT_CONTEXT_VALUE_LEN 10 /* "Signature1" */
#define CDI_SIG_STRUCT_CONTEXT_LEN (1 + CDI_SIG_STRUCT_CONTEXT_VALUE_LEN)
#define CDI_SIG_STRUCT_CONTEXT { \
    CBOR_HDR1(kCborBstr, CDI_SIG_STRUCT_CONTEXT_VALUE_LEN), \
    'S', 'i', 'g', 'n', 'a', 't', 'u', 'r', 'e', '1' \
}

typedef struct {
    // Array header: 4 elements
    uint8_t array_hdr;
    // 1. Context: tstr("Signature1")
    uint8_t context[CDI_SIG_STRUCT_CONTEXT_LEN];
    // 2. Body protected: bstr(COSE param)
    cose_param_bstr_t body_protected;
    // 3. External AAD: bstr(0 bytes)
    uint8_t external_aad;
    // 4. Payload - not fixed, contained in cdi_sig_struct_t
} cdi_sig_struct_fixed_hdr_t;

typedef struct {
    cdi_sig_struct_fixed_hdr_t fixed_hdr;
    cwt_claims_bstr_t payload;
} cdi_sig_struct_t;

////////////////////////////////////////////////////////////////////////////////
// CDI certificate = COSE_Sign1 structure - see go/al-dice

typedef struct {
    // Array header: 4 elements
    uint8_t array_hdr;
    // 1. Protected: bstr(COSE param)
    cose_param_bstr_t protected;
    // 2. Unprotected: empty map
    uint8_t unprotected;
    // 3. Payload: bstr(CWT claims) - not fixed, contained in cdi_cert_t
    // 4. Signature: bstr(64 bytes) - not fixed, contained in cdi_cert_t
} cdi_cert_fixed_hdr_t;

typedef struct {
    cdi_cert_fixed_hdr_t fixed_hdr;
    cwt_claims_bstr_t payload;
    cbor_bstr64_t signature;
} cdi_cert_t;

////////////////////////////////////////////////////////////////////////////////
// Common structure to build Sig structure or CDI cert
typedef struct {
    // add padding to make it the same size as cdi_sig_struct_fixed_hdr_t
    uint8_t padding[CDI_SIG_STRUCT_CONTEXT_LEN];
    cdi_cert_fixed_hdr_t fixed_hdr;
} cdi_cert_extended_fixed_hdr_t;

// static_assert( sizeof(cdi_sig_struct_fixed_hdr_t) == sizeof(cdi_cert_extended_fixed_hdr_t) )

typedef union {
    cdi_sig_struct_fixed_hdr_t sig_struct;
    cdi_cert_extended_fixed_hdr_t cdi_cert;
} cdi_header_options_t;

typedef struct {
    cdi_header_options_t hdr;
    cwt_claims_bstr_t payload;
    cbor_bstr64_t signature;
} cdi_builder_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __GSC_UTILS_DICE_CBOR_DICE_H */
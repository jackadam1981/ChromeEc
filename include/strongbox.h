/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __INCLUDE_STRONGBOX_CMDS_H
#define __INCLUDE_STRONGBOX_CMDS_H

#include "common.h" /* For __packed. */
#include "extension.h"

/**
 * Command names and values are taken from declare_req_rsp_enums! in:
 * https://cs.android.com/android/platform/superproject/main/+/main:system/keymint/wire/src/types.rs
 */
enum strongbox_cmd_cc {
	SB_DeviceGetHardwareInfo = 0x11,
	SB_DeviceAddRngEntropy = 0x12,
	SB_DeviceGenerateKey = 0x13,
	SB_DeviceImportKey = 0x14,
	SB_DeviceImportWrappedKey = 0x15,
	SB_DeviceUpgradeKey = 0x16,
	SB_DeviceDeleteKey = 0x17,
	SB_DeviceDeleteAllKeys = 0x18,
	SB_DeviceDestroyAttestationIds = 0x19,
	SB_DeviceBegin = 0x1a,
	SB_DeviceEarlyBootEnded = 0x1c,
	SB_DeviceConvertStorageKeyToEphemeral = 0x1d,
	SB_DeviceGetKeyCharacteristics = 0x1e,
	SB_OperationUpdateAad = 0x31,
	SB_OperationUpdate = 0x32,
	SB_OperationFinish = 0x33,
	SB_OperationAbort = 0x34,
	SB_RpcGetHardwareInfo = 0x41,
	SB_RpcGenerateEcdsaP256KeyPair = 0x42,
	SB_RpcGenerateCertificateRequest = 0x43,
	SB_RpcGenerateCertificateV2Request = 0x44,
	SB_SharedSecretGetSharedSecretParameters = 0x51,
	SB_SharedSecretComputeSharedSecret = 0x52,
	SB_SecureClockGenerateTimeStamp = 0x61,
	SB_GetRootOfTrustChallenge = 0x71,
	SB_GetRootOfTrust = 0x72,
	SB_SendRootOfTrust = 0x73,
	SB_SetHalInfo = 0x81,
	SB_SetBootInfo = 0x82,
	SB_SetAttestationIds = 0x83,
	SB_SetHalVersion = 0x84,
	SB_SetAdditionalAttestationInfo = 0x91,
};

/**
 * Error names and values are taken from ErrorCode enum in:
 * https://cs.android.com/android/platform/superproject/main/+/main:system/keymint/wire/src/keymint.rs
 */
enum strongbox_error {
	SB_OK = 0,
	SBERR_RootOfTrustAlreadySet = -1,
	SBERR_UnsupportedPurpose = -2,
	SBERR_IncompatiblePurpose = -3,
	SBERR_UnsupportedAlgorithm = -4,
	SBERR_IncompatibleAlgorithm = -5,
	SBERR_UnsupportedKeySize = -6,
	SBERR_UnsupportedBlockMode = -7,
	SBERR_IncompatibleBlockMode = -8,
	SBERR_UnsupportedMacLength = -9,
	SBERR_UnsupportedPaddingMode = -10,
	SBERR_IncompatiblePaddingMode = -11,
	SBERR_UnsupportedDigest = -12,
	SBERR_IncompatibleDigest = -13,
	SBERR_InvalidExpirationTime = -14,
	SBERR_InvalidUserId = -15,
	SBERR_InvalidAuthorizationTimeout = -16,
	SBERR_UnsupportedKeyFormat = -17,
	SBERR_IncompatibleKeyFormat = -18,
	SBERR_UnsupportedKeyEncryptionAlgorithm = -19,
	SBERR_UnsupportedKeyVerificationAlgorithm = -20,
	SBERR_InvalidInputLength = -21,
	SBERR_KeyExportOptionsInvalid = -22,
	SBERR_DelegationNotAllowed = -23,
	SBERR_KeyNotYetValid = -24,
	SBERR_KeyExpired = -25,
	SBERR_KeyUserNotAuthenticated = -26,
	SBERR_OutputParameterNull = -27,
	SBERR_InvalidOperationHandle = -28,
	SBERR_InsufficientBufferSpace = -29,
	SBERR_VerificationFailed = -30,
	SBERR_TooManyOperations = -31,
	SBERR_UnexpectedNullPointer = -32,
	SBERR_InvalidKeyBlob = -33,
	SBERR_ImportedKeyNotEncrypted = -34,
	SBERR_ImportedKeyDecryptionFailed = -35,
	SBERR_ImportedKeyNotSigned = -36,
	SBERR_ImportedKeyVerificationFailed = -37,
	SBERR_InvalidArgument = -38,
	SBERR_UnsupportedTag = -39,
	SBERR_InvalidTag = -40,
	SBERR_MemoryAllocationFailed = -41,
	SBERR_ImportParameterMismatch = -44,
	SBERR_SecureHwAccessDenied = -45,
	SBERR_OperationCancelled = -46,
	SBERR_ConcurrentAccessConflict = -47,
	SBERR_SecureHwBusy = -48,
	SBERR_SecureHwCommunicationFailed = -49,
	SBERR_UnsupportedEcField = -50,
	SBERR_MissingNonce = -51,
	SBERR_InvalidNonce = -52,
	SBERR_MissingMacLength = -53,
	SBERR_KeyRateLimitExceeded = -54,
	SBERR_CallerNonceProhibited = -55,
	SBERR_KeyMaxOpsExceeded = -56,
	SBERR_InvalidMacLength = -57,
	SBERR_MissingMinMacLength = -58,
	SBERR_UnsupportedMinMacLength = -59,
	SBERR_UnsupportedKdf = -60,
	SBERR_UnsupportedEcCurve = -61,
	SBERR_KeyRequiresUpgrade = -62,
	SBERR_AttestationChallengeMissing = -63,
	SBERR_KeymintNotConfigured = -64,
	SBERR_AttestationApplicationIdMissing = -65,
	SBERR_CannotAttestIds = -66,
	SBERR_RollbackResistanceUnavailable = -67,
	SBERR_HardwareTypeUnavailable = -68,
	SBERR_ProofOfPresenceRequired = -69,
	SBERR_ConcurrentProofOfPresenceRequested = -70,
	SBERR_NoUserConfirmation = -71,
	SBERR_DeviceLocked = -72,
	SBERR_EarlyBootEnded = -73,
	SBERR_AttestationKeysNotProvisioned = -74,
	SBERR_AttestationIdsNotProvisioned = -75,
	SBERR_InvalidOperation = -76,
	SBERR_StorageKeyUnsupported = -77,
	SBERR_IncompatibleMgfDigest = -78,
	SBERR_UnsupportedMgfDigest = -79,
	SBERR_MissingNotBefore = -80,
	SBERR_MissingNotAfter = -81,
	SBERR_MissingIssuerSubject = -82,
	SBERR_InvalidIssuerSubject = -83,
	SBERR_BootLevelExceeded = -84,
	SBERR_HardwareNotYetAvailable = -85,
	SBERR_ModuleHashAlreadySet = -86,
	SBERR_Unimplemented = -100,
	SBERR_VersionMismatch = -101,
	SBERR_UnknownError = -1000,
};

/* Type of function handling extension commands. */
typedef enum strongbox_error (*strongbox_handler)(
	struct vendor_cmd_params *params);

/* Pointer table */
struct strongbox_command {
	uint16_t command_code;
	strongbox_handler handler;
} __packed;

/* Vendor command which takes params as struct */
#define DECLARE_STRONGBOX_COMMAND(cmd_code, func)                     \
	const struct strongbox_command __keep __no_sanitize_address   \
		__vendor_cmd_##cmd_code                               \
		__attribute__((section(".rodata.strongboxcmds"))) = { \
			.command_code = cmd_code, .handler = func     \
		}

/**
 * Find handler for an StrongBox extension command.
 *
 * Use the interface specific function call in order to check the policies for
 * handling the commands on that interface.
 *
 * @param p		Parameters for the command
 * @return The return code from processing the command.
 */
uint32_t extension_route_strongbox_command(struct vendor_cmd_params *p);

// Defines from the missing bitfields.rs, inferred from Android source.
#define TAGTYPE_SHIFT 28
#define TAGTYPE_MASK_UNSHIFTED 0xF
#define TAGTYPE_MASK (TAGTYPE_MASK_UNSHIFTED << TAGTYPE_SHIFT)
#define TAG_ID_SHIFT 0
#define TAG_ID_MASK 0x0FFFFFFF

// Corresponds to Rust's `TagTypeEnum`
typedef enum {
    TAG_TYPE_ENUM_INVALID  = 0,
    TAG_TYPE_ENUM_ENUM     = 1,
    TAG_TYPE_ENUM_ENUM_REP = 2,
    TAG_TYPE_ENUM_UINT     = 3,
    TAG_TYPE_ENUM_UINT_REP = 4,
    TAG_TYPE_ENUM_ULONG    = 5,
    TAG_TYPE_ENUM_DATE     = 6,
    TAG_TYPE_ENUM_BOOL     = 7,
    TAG_TYPE_ENUM_BIGNUM   = 8,
    TAG_TYPE_ENUM_BYTES    = 9,
    TAG_TYPE_ENUM_ULONG_REP = 10,
} TagTypeEnum;

// Corresponds to Rust's `TagType`. These are the shifted values.
typedef enum {
    TAG_TYPE_INVALID  = (TAG_TYPE_ENUM_INVALID   << TAGTYPE_SHIFT),
    TAG_TYPE_ENUM     = (TAG_TYPE_ENUM_ENUM      << TAGTYPE_SHIFT),
    TAG_TYPE_ENUM_REP = (TAG_TYPE_ENUM_ENUM_REP  << TAGTYPE_SHIFT),
    TAG_TYPE_UINT     = (TAG_TYPE_ENUM_UINT      << TAGTYPE_SHIFT),
    TAG_TYPE_UINT_REP = (TAG_TYPE_ENUM_UINT_REP  << TAGTYPE_SHIFT),
    TAG_TYPE_ULONG    = (TAG_TYPE_ENUM_ULONG     << TAGTYPE_SHIFT),
    TAG_TYPE_DATE     = (TAG_TYPE_ENUM_DATE      << TAGTYPE_SHIFT),
    TAG_TYPE_BOOL     = (TAG_TYPE_ENUM_BOOL      << TAGTYPE_SHIFT),
    TAG_TYPE_BIGNUM   = (TAG_TYPE_ENUM_BIGNUM    << TAGTYPE_SHIFT),
    TAG_TYPE_BYTES    = (TAG_TYPE_ENUM_BYTES     << TAGTYPE_SHIFT),
    TAG_TYPE_ULONG_REP = (TAG_TYPE_ENUM_ULONG_REP << TAGTYPE_SHIFT),
} TagType;


// Corresponds to Rust's `Tag`. All known KeyMint tags are defined here.
typedef enum {
    TAG_INVALID                         = 0,
    TAG_PURPOSE                         = (TAG_TYPE_ENUM_REP | 1),
    TAG_ALGORITHM                       = (TAG_TYPE_ENUM | 2),
    TAG_KEY_SIZE                        = (TAG_TYPE_UINT | 3),
    TAG_BLOCK_MODE                      = (TAG_TYPE_ENUM_REP | 4),
    TAG_DIGEST                          = (TAG_TYPE_ENUM_REP | 5),
    TAG_PADDING                         = (TAG_TYPE_ENUM_REP | 6),
    TAG_CALLER_NONCE                    = (TAG_TYPE_BOOL | 7),
    TAG_MIN_MAC_LENGTH                  = (TAG_TYPE_UINT | 8),
    TAG_EC_CURVE                        = (TAG_TYPE_ENUM | 10),
    TAG_RSA_PUBLIC_EXPONENT             = (TAG_TYPE_ULONG | 200),
    TAG_INCLUDE_UNIQUE_ID               = (TAG_TYPE_BOOL | 202),
    TAG_RSA_OAEP_MGF_DIGEST             = (TAG_TYPE_ENUM_REP | 203),
    TAG_BOOTLOADER_ONLY                 = (TAG_TYPE_BOOL | 302),
    TAG_ROLLBACK_RESISTANCE             = (TAG_TYPE_BOOL | 303),
    TAG_HARDWARE_TYPE                   = (TAG_TYPE_ENUM | 304),
    TAG_EARLY_BOOT_ONLY                 = (TAG_TYPE_BOOL | 305),
    TAG_ACTIVE_DATETIME                 = (TAG_TYPE_DATE | 400),
    TAG_ORIGINATION_EXPIRE_DATETIME     = (TAG_TYPE_DATE | 401),
    TAG_USAGE_EXPIRE_DATETIME           = (TAG_TYPE_DATE | 402),
    TAG_MIN_SECONDS_BETWEEN_OPS         = (TAG_TYPE_UINT | 403),
    TAG_MAX_USES_PER_BOOT               = (TAG_TYPE_UINT | 404),
    TAG_USAGE_COUNT_LIMIT               = (TAG_TYPE_UINT | 405),
    TAG_USER_ID                         = (TAG_TYPE_UINT | 501),
    TAG_USER_SECURE_ID                  = (TAG_TYPE_ULONG_REP | 502),
    TAG_NO_AUTH_REQUIRED                = (TAG_TYPE_BOOL | 503),
    TAG_USER_AUTH_TYPE                  = (TAG_TYPE_ENUM | 504),
    TAG_AUTH_TIMEOUT                    = (TAG_TYPE_UINT | 505),
    TAG_ALLOW_WHILE_ON_BODY             = (TAG_TYPE_BOOL | 506),
    TAG_TRUSTED_USER_PRESENCE_REQUIRED  = (TAG_TYPE_BOOL | 507),
    TAG_TRUSTED_CONFIRMATION_REQUIRED   = (TAG_TYPE_BOOL | 508),
    TAG_UNLOCKED_DEVICE_REQUIRED        = (TAG_TYPE_BOOL | 509),
    TAG_APPLICATION_ID                  = (TAG_TYPE_BYTES | 601),
    TAG_APPLICATION_DATA                = (TAG_TYPE_BYTES | 700),
    TAG_CREATION_DATETIME               = (TAG_TYPE_DATE | 701),
    TAG_ORIGIN                          = (TAG_TYPE_ENUM | 702),
    TAG_ROOT_OF_TRUST                   = (TAG_TYPE_BYTES | 704),
    TAG_OS_VERSION                      = (TAG_TYPE_UINT | 705),
    TAG_OS_PATCHLEVEL                   = (TAG_TYPE_UINT | 706),
    TAG_UNIQUE_ID                       = (TAG_TYPE_BYTES | 707),
    TAG_ATTESTATION_CHALLENGE           = (TAG_TYPE_BYTES | 708),
    TAG_ATTESTATION_APPLICATION_ID      = (TAG_TYPE_BYTES | 709),
    TAG_ATTESTATION_ID_BRAND            = (TAG_TYPE_BYTES | 710),
    TAG_ATTESTATION_ID_DEVICE           = (TAG_TYPE_BYTES | 711),
    TAG_ATTESTATION_ID_PRODUCT          = (TAG_TYPE_BYTES | 712),
    TAG_ATTESTATION_ID_SERIAL           = (TAG_TYPE_BYTES | 713),
    TAG_ATTESTATION_ID_IMEI             = (TAG_TYPE_BYTES | 714),
    TAG_ATTESTATION_ID_MEID             = (TAG_TYPE_BYTES | 715),
    TAG_ATTESTATION_ID_MANUFACTURER     = (TAG_TYPE_BYTES | 716),
    TAG_ATTESTATION_ID_MODEL            = (TAG_TYPE_BYTES | 717),
    TAG_VENDOR_PATCHLEVEL               = (TAG_TYPE_UINT | 718),
    TAG_BOOT_PATCHLEVEL                 = (TAG_TYPE_UINT | 719),
    TAG_DEVICE_UNIQUE_ATTESTATION       = (TAG_TYPE_BOOL | 720),
    TAG_IDENTITY_CREDENTIAL_KEY         = (TAG_TYPE_BOOL | 721),
    TAG_STORAGE_KEY                     = (TAG_TYPE_BOOL | 722),
    TAG_ATTESTATION_ID_SECOND_IMEI      = (TAG_TYPE_BYTES | 723),
    TAG_MODULE_HASH                     = (TAG_TYPE_BYTES | 724),
    TAG_ASSOCIATED_DATA                 = (TAG_TYPE_BYTES | 1000),
    TAG_NONCE                           = (TAG_TYPE_BYTES | 1001),
    TAG_MAC_LENGTH                      = (TAG_TYPE_UINT | 1003),
    TAG_RESET_SINCE_ID_ROTATION         = (TAG_TYPE_BOOL | 1004),
    TAG_CONFIRMATION_TOKEN              = (TAG_TYPE_BYTES | 1005),
    TAG_CERTIFICATE_SERIAL              = (TAG_TYPE_BIGNUM | 1006),
    TAG_CERTIFICATE_SUBJECT             = (TAG_TYPE_BYTES | 1007),
    TAG_CERTIFICATE_NOT_BEFORE          = (TAG_TYPE_DATE | 1008),
    TAG_CERTIFICATE_NOT_AFTER           = (TAG_TYPE_DATE | 1009),
    TAG_MAX_BOOT_LEVEL                  = (TAG_TYPE_UINT | 1010),
} keymint_tag_t;

// --- Inlined helper functions ---

/// Returns the `TagType` of this tag.
static inline TagType tag_get_type(keymint_tag_t tag) {
    return (TagType)(tag & TAGTYPE_MASK);
}

/// Returns the un-shifted `TagTypeEnum` of this tag.
static inline TagTypeEnum tag_get_type_enum(keymint_tag_t tag) {
    return (TagTypeEnum)((tag & TAGTYPE_MASK) >> TAGTYPE_SHIFT);
}

/// Returns the tag ID of this tag.
static inline uint32_t tag_get_id(keymint_tag_t tag) {
    return (tag >> TAG_ID_SHIFT) & TAG_ID_MASK;
}

// --- Function Prototypes for tag property checks ---

/// Check if tag is explicitly disallowed for Generate/Import
bool is_gen_disallowed(keymint_tag_t tag);

/// Check if a tag should be enforced by hardware (Strongbox).
/// This is a simplified implementation based on the documentation in tags.rs.
bool is_hw_enforced(keymint_tag_t tag);

/// Check if a tag should be enforced by software (Keystore).
/// This is a simplified implementation based on the documentation in tags.rs.
bool is_sw_enforced(keymint_tag_t tag);

// Corresponds to `Keymint` struct. Holds device state.
typedef struct {
    uint32_t os_version;
    uint32_t os_patchlevel;
    uint32_t vendor_patchlevel;
    uint32_t boot_patchlevel;
    uint32_t hmac_tag_key[8];
    uint32_t drbg_seed_key[8];
} Keymint_t;

// Helper struct for optional u32
typedef struct {
    bool is_set;
    uint32_t value;
} optional_u32_t;

// Helper struct for optional u64
typedef struct {
    bool is_set;
    uint64_t value;
} optional_u64_t;

// Helper struct for optional byte slice
typedef struct {
    bool is_set;
    const uint8_t* data;
    size_t len;
} optional_slice_t;

// Corresponds to `KeyAttributes` struct. Filled during tag parsing.
typedef struct {
    uint32_t purpose_flags;
    int32_t algorithm; // Using an int to hold Algorithm enum, with -1 as unset
    optional_u32_t key_size;
    optional_u32_t curve_id;
    bool caller_nonce;
    optional_u64_t rsa_exponent;
    optional_slice_t application_id;
    optional_slice_t application_data;
} KeyAttributes_t;

// Corresponds to `TagData` concept for parsing.
typedef struct {
    keymint_tag_t tag;
    const uint32_t* value_ptr; // Points to the start of the value in the buffer
    size_t total_word_len;     // Total length of the tag entry (tag + value) in words
} TagData_t;


// Other required enums from `crate::enums`
typedef enum {
    ALGORITHM_NONE = 0,
    ALGORITHM_RSA = 1,
    ALGORITHM_EC = 3,
    ALGORITHM_AES = 32,
    ALGORITHM_TDES = 33,
    ALGORITHM_HMAC = 128,
} Algorithm;

typedef enum {
    KEY_PURPOSE_ENCRYPT = 0,
    KEY_PURPOSE_DECRYPT = 1,
    KEY_PURPOSE_SIGN = 2,
    KEY_PURPOSE_VERIFY = 3,
    KEY_PURPOSE_WRAP_KEY = 5,
    KEY_PURPOSE_AGREE_KEY = 6,
    KEY_PURPOSE_ATTEST_KEY = 7,
} KeyPurpose;

typedef enum {
    EC_CURVE_P_224 = 0,
    EC_CURVE_P_256 = 1,
    EC_CURVE_P_384 = 2,
    EC_CURVE_P_521 = 3,
} EcCurve;

typedef enum {
    SECURITY_LEVEL_SOFTWARE = 0,
    SECURITY_LEVEL_TRUSTED_ENVIRONMENT = 1,
    SECURITY_LEVEL_STRONGBOX = 2,
    SECURITY_LEVEL_KEYSTORE = 100,
} SecurityLevel;

typedef enum {
    KEY_ORIGIN_GENERATED = 0,
    KEY_ORIGIN_IMPORTED = 1,
    KEY_ORIGIN_UNKNOWN = 2,
    KEY_ORIGIN_SECURELY_IMPORTED = 3,
} KeyOrigin;


#endif /* __INCLUDE_STRONGBOX_CMDS_H */

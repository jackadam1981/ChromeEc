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

#endif /* __INCLUDE_STRONGBOX_CMDS_H */

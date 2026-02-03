# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for testing TPM2 commands."""

import struct

import cbor
import subcmd
import x509keymint


TPM_SU_CLEAR = 0x0000
TPM_SU_STATE = 0x0001

SB_DeviceGetHardwareInfo = 0x11
SB_DeviceAddRngEntropy = 0x12
SB_DeviceGenerateKey = 0x13
SB_DeviceImportKey = 0x14
SB_DeviceImportWrappedKey = 0x15
SB_DeviceUpgradeKey = 0x16
SB_DeviceDeleteKey = 0x17
SB_DeviceDeleteAllKeys = 0x18
SB_DeviceDestroyAttestationIds = 0x19
SB_DeviceBegin = 0x1A
SB_DeviceEarlyBootEnded = 0x1C
SB_DeviceConvertStorageKeyToEphemeral = 0x1D
SB_DeviceGetKeyCharacteristics = 0x1E
SB_OperationUpdateAad = 0x31
SB_OperationUpdate = 0x32
SB_OperationFinish = 0x33
SB_OperationAbort = 0x34
SB_RpcGetHardwareInfo = 0x41
SB_RpcGenerateEcdsaP256KeyPair = 0x42
SB_RpcGenerateCertificateRequest = 0x43
SB_RpcGenerateCertificateV2Request = 0x44
SB_SharedSecretGetSharedSecretParameters = 0x51
SB_SharedSecretComputeSharedSecret = 0x52
SB_SecureClockGenerateTimeStamp = 0x61
SB_GetRootOfTrustChallenge = 0x71
SB_GetRootOfTrust = 0x72
SB_SendRootOfTrust = 0x73
SB_SetHalInfo = 0x81
SB_SetBootInfo = 0x82
SB_SetAttestationIds = 0x83
SB_SetHalVersion = 0x84
SB_SetAdditionalAttestationInfo = 0x91
SB_GetDiceChain = 0xA0
SB_SetHalBootInfo = 0xA1
SB_SetStrongboxState = 0xA2

# Keymint tag types

KM_TAG_TYPE_INVALID = 0
KM_TAG_TYPE_ENUM = 1
KM_TAG_TYPE_ENUM_REP = 2
KM_TAG_TYPE_UINT = 3
KM_TAG_TYPE_UINT_REP = 4
KM_TAG_TYPE_ULONG = 5
KM_TAG_TYPE_DATE = 6
KM_TAG_TYPE_BOOL = 7
KM_TAG_TYPE_BIGNUM = 8
KM_TAG_TYPE_BYTES = 9
KM_TAG_TYPE_ULONG_REP = 10

KM_TAG_TYPE_SHIFT = 28

KM_TYPE_INVALID = KM_TAG_TYPE_INVALID << KM_TAG_TYPE_SHIFT
KM_TYPE_ENUM = KM_TAG_TYPE_ENUM << KM_TAG_TYPE_SHIFT
KM_TYPE_ENUM_REP = KM_TAG_TYPE_ENUM_REP << KM_TAG_TYPE_SHIFT
KM_TYPE_UINT = KM_TAG_TYPE_UINT << KM_TAG_TYPE_SHIFT
KM_TYPE_UINT_REP = KM_TAG_TYPE_UINT_REP << KM_TAG_TYPE_SHIFT
KM_TYPE_ULONG = KM_TAG_TYPE_ULONG << KM_TAG_TYPE_SHIFT
KM_TYPE_DATE = KM_TAG_TYPE_DATE << KM_TAG_TYPE_SHIFT
KM_TYPE_BOOL = KM_TAG_TYPE_BOOL << KM_TAG_TYPE_SHIFT
KM_TYPE_BIGNUM = KM_TAG_TYPE_BIGNUM << KM_TAG_TYPE_SHIFT
KM_TYPE_BYTES = KM_TAG_TYPE_BYTES << KM_TAG_TYPE_SHIFT
KM_TYPE_ULONG_REP = KM_TAG_TYPE_ULONG_REP << KM_TAG_TYPE_SHIFT
KM_TAG_TYPE_MASK = 0xF << KM_TAG_TYPE_SHIFT


# Hardware-enforced tags
KM_TAG_PURPOSE = KM_TYPE_ENUM_REP | 1
KM_TAG_ALGORITHM = KM_TYPE_ENUM | 2
KM_TAG_KEY_SIZE = KM_TYPE_UINT | 3
KM_TAG_BLOCK_MODE = KM_TYPE_ENUM_REP | 4
KM_TAG_DIGEST = KM_TYPE_ENUM_REP | 5
KM_TAG_PADDING = KM_TYPE_ENUM_REP | 6
KM_TAG_CALLER_NONCE = KM_TYPE_BOOL | 7
KM_TAG_MIN_MAC_LENGTH = KM_TYPE_UINT | 8
KM_TAG_EC_CURVE = KM_TYPE_ENUM | 10
KM_TAG_RSA_PUBLIC_EXPONENT = KM_TYPE_ULONG | 200
KM_TAG_INCLUDE_UNIQUE_ID = KM_TYPE_BOOL | 202
KM_TAG_RSA_OAEP_MGF_DIGEST = KM_TYPE_ENUM_REP | 203
KM_TAG_BOOTLOADER_ONLY = KM_TYPE_BOOL | 302
KM_TAG_ROLLBACK_RESISTANCE = KM_TYPE_BOOL | 303
KM_TAG_HARDWARE_TYPE = KM_TYPE_ENUM | 304
KM_TAG_EARLY_BOOT_ONLY = KM_TYPE_BOOL | 305
KM_TAG_MAX_USES_PER_BOOT = KM_TYPE_UINT | 404
KM_TAG_USAGE_COUNT_LIMIT = KM_TYPE_UINT | 405
KM_TAG_USER_SECURE_ID = KM_TYPE_ULONG_REP | 502
KM_TAG_NO_AUTH_REQUIRED = KM_TYPE_BOOL | 503
KM_TAG_USER_AUTH_TYPE = KM_TYPE_ENUM | 504
KM_TAG_AUTH_TIMEOUT = KM_TYPE_UINT | 505
KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED = KM_TYPE_BOOL | 507
KM_TAG_TRUSTED_CONFIRMATION_REQUIRED = KM_TYPE_BOOL | 508
KM_TAG_UNLOCKED_DEVICE_REQUIRED = KM_TYPE_BOOL | 509
KM_TAG_ORIGIN = KM_TYPE_ENUM | 702
KM_TAG_OS_VERSION = KM_TYPE_UINT | 705
KM_TAG_OS_PATCHLEVEL = KM_TYPE_UINT | 706
KM_TAG_UNIQUE_ID = KM_TYPE_BYTES | 707
KM_TAG_VENDOR_PATCHLEVEL = KM_TYPE_UINT | 718
KM_TAG_BOOT_PATCHLEVEL = KM_TYPE_UINT | 719
KM_TAG_DEVICE_UNIQUE_ATTESTATION = KM_TYPE_BOOL | 720
KM_TAG_IDENTITY_CREDENTIAL_KEY = KM_TYPE_BOOL | 721
KM_TAG_STORAGE_KEY = KM_TYPE_BOOL | 722
KM_TAG_MAC_LENGTH = KM_TYPE_UINT | 1003
KM_TAG_MAX_BOOT_LEVEL = KM_TYPE_UINT | 1010

# Software-enforced tags
KM_TAG_ACTIVE_DATETIME = KM_TYPE_DATE | 400
KM_TAG_ORIGINATION_EXPIRE_DATETIME = KM_TYPE_DATE | 401
KM_TAG_USAGE_EXPIRE_DATETIME = KM_TYPE_DATE | 402
KM_TAG_USER_ID = KM_TYPE_UINT | 501
KM_TAG_ALLOW_WHILE_ON_BODY = KM_TYPE_BOOL | 506
KM_TAG_CREATION_DATETIME = KM_TYPE_DATE | 701
KM_TAG_ATTESTATION_APPLICATION_ID = KM_TYPE_BYTES | 709

# Input-only / special tags
KM_TAG_MIN_SECONDS_BETWEEN_OPS = KM_TYPE_UINT | 403
KM_TAG_APPLICATION_ID = KM_TYPE_BYTES | 601
KM_TAG_APPLICATION_DATA = KM_TYPE_BYTES | 700
KM_TAG_ROOT_OF_TRUST = KM_TYPE_BYTES | 704
KM_TAG_ATTESTATION_CHALLENGE = KM_TYPE_BYTES | 708
KM_TAG_ATTESTATION_ID_BRAND = KM_TYPE_BYTES | 710
KM_TAG_ATTESTATION_ID_DEVICE = KM_TYPE_BYTES | 711
KM_TAG_ATTESTATION_ID_PRODUCT = KM_TYPE_BYTES | 712
KM_TAG_ATTESTATION_ID_SERIAL = KM_TYPE_BYTES | 713
KM_TAG_ATTESTATION_ID_IMEI = KM_TYPE_BYTES | 714
KM_TAG_ATTESTATION_ID_MEID = KM_TYPE_BYTES | 715
KM_TAG_ATTESTATION_ID_MANUFACTURER = KM_TYPE_BYTES | 716
KM_TAG_ATTESTATION_ID_MODEL = KM_TYPE_BYTES | 717
KM_TAG_ATTESTATION_ID_SECOND_IMEI = KM_TYPE_BYTES | 723
KM_TAG_MODULE_HASH = KM_TYPE_BYTES | 724
KM_TAG_ASSOCIATED_DATA = KM_TYPE_BYTES | 1000
KM_TAG_NONCE = KM_TYPE_BYTES | 1001
KM_TAG_RESET_SINCE_ID_ROTATION = KM_TYPE_BOOL | 1004
KM_TAG_CONFIRMATION_TOKEN = KM_TYPE_BYTES | 1005
KM_TAG_CERTIFICATE_SERIAL = KM_TYPE_BIGNUM | 1006
KM_TAG_CERTIFICATE_SUBJECT = KM_TYPE_BYTES | 1007
KM_TAG_CERTIFICATE_NOT_BEFORE = KM_TYPE_DATE | 1008
KM_TAG_CERTIFICATE_NOT_AFTER = KM_TYPE_DATE | 1009

KM_ALG_NONE = 0
KM_ALG_RSA = 1
KM_ALG_EC = 3
KM_ALG_AES = 32
KM_ALG_TDES = 33
KM_ALG_HMAC = 128

KM_PURPOSE_ENCRYPT = 0
KM_PURPOSE_DECRYPT = 1
KM_PURPOSE_SIGN = 2
KM_PURPOSE_VERIFY = 3
KM_PURPOSE_WRAP_KEY = 5
KM_PURPOSE_AGREE_KEY = 6
KM_PURPOSE_ATTEST_KEY = 7

KM_SECURITY_SOFTWARE = 0
KM_SECURITY_TRUSTED_ENVIRONMENT = 1
KM_SECURITY_STRONGBOX = 2
KM_SECURITY_KEYSTORE = 100


KM_ORIGIN_GENERATED = 0
KM_ORIGIN_DERIVED = 1
KM_ORIGIN_IMPORTED = 2
KM_ORIGIN_RESERVED = 3
KM_ORIGIN_SECURELY_IMPORTED = 4

KM_DIGEST_NONE = 0
KM_DIGEST_MD5 = 1
KM_DIGEST_SHA1 = 2
KM_DIGEST_SHA_2_224 = 3
KM_DIGEST_SHA_2_256 = 4
KM_DIGEST_SHA_2_384 = 5
KM_DIGEST_SHA_2_512 = 6


def U32(val: int):
    return val.to_bytes(4, byteorder="little")


def U64(val: int):
    return val.to_bytes(8, byteorder="little")


def Tag(tag: int, value):
    tag_type = tag & KM_TAG_TYPE_MASK
    tag_val = tag & ~KM_TAG_TYPE_MASK
    if tag_type in [
        KM_TYPE_ENUM,
        KM_TYPE_ENUM_REP,
        KM_TYPE_UINT,
        KM_TYPE_UINT_REP,
    ]:
        if not isinstance(value, int):
            print(f"Tag {tag_val} Expected int got {type(value)}, {value}")
        return U32(tag) + U32(value)
    if tag_type in [KM_TYPE_BIGNUM, KM_TYPE_BYTES]:
        if not isinstance(value, bytes):
            print(f"Tag {tag_val} Expected bytes got {type(value)}, {value}")
        # Encode length into tag and pad value to 32-bit
        return (
            U32(tag | (len(value) << 16))
            + value
            + bytes((4 - len(value) % 4) % 4)
        )
    if tag_type in [KM_TYPE_DATE, KM_TYPE_ULONG, KM_TYPE_ULONG_REP]:
        if not isinstance(value, int):
            print(f"Tag {tag_val} Expected int got {type(value)}, {value}")
        return U32(tag) + U64(value)
    if tag_type == KM_TYPE_BOOL:
        return U32(tag)
    print(f"Invalid tag type: {tag_type}")
    return None


def tpm2_startup(tpm, state):
    """Send TPM2_Startup command

    Args:
        tpm: a tpm object used to communicate with the device
        state: TPM_SU_CLEAR / TPM_SU_STATE

    Raises:
        subcmd.TpmTestError: on unexpected target responses
    """
    startup = [
        0x80,
        0x01,  # TPM_ST_NO_SESSIONS
        0x00,
        0x00,
        0x00,
        0x0C,  # commandSize = 12
        0x00,
        0x00,
        0x01,
        0x44,  # TPM_CC_Startup
        0x00,
        0x00,  # TPM_SU_CLEAR / TPM_SU_STATE
    ]
    startup[10:12] = state.to_bytes(2, byteorder="big")
    cmd = bytes(startup)
    response = tpm.command(cmd)
    return response


def tpm2_shutdown(tpm, state):
    """Send TPM2_Shutdown command

    Args:
        tpm: a tpm object used to communicate with the device
        state: TPM_SU_CLEAR / TPM_SU_STATE

    Raises:
        subcmd.TpmTestError: on unexpected target responses
    """
    shutdown = [
        0x80,
        0x01,  # tag: TPM_ST_NO_SESSIONS
        0x00,
        0x00,
        0x00,
        0x0C,  # size:
        0x00,
        0x00,
        0x01,
        0x45,  # command: TPM_CC_Shutdown
        # ======
        0x00,
        0x00,  # shutdown type: TPM_SU_STATE
    ]
    shutdown[10:12] = state.to_bytes(2, byteorder="big")
    cmd = bytes(shutdown)
    response = tpm.command(cmd)
    return response


def wrap_sb_command(subcmd_code, cmd_body):
    """Wrap TPM command into extension command header"""
    return (
        struct.pack(
            ">H2IH",
            0x8001,
            len(cmd_body) + struct.calcsize(">H2IH"),
            0x20000001,
            subcmd_code,
        )
        + cmd_body
    )


def unwrap_ext_response(self, expected_subcmd, response):
    """Verify basic validity and strip off TPM extended command header.

    Get the response generated by the device, as it came off the wire,
    verify that header fields match expectations, then strip off the
    extension command header and return the payload to the caller.

    Args:
        expected_subcmd: an int, up to 16 bits in size, the extension
            command this response is supposed to be for.
        response: a binary string, the actual response received
            over the wire.

    Returns:
        the binary string of the response payload,
        if validation succeeded.

    Raises:
        subcmd.TpmTestError: in case there are any validation problems,
        the error message describes the problem.
    """
    header_size = struct.calcsize(">H2IH")
    tag, size, cmd, sub = struct.unpack(">H2IH", response[:header_size])
    if tag != 0x8001:
        raise subcmd.TpmTestError("Wrong response tag: %4.4x" % tag)
    if cmd:
        raise subcmd.TpmTestError(
            "Unexpected response command" " field: %8.8x" % cmd
        )
    if sub != expected_subcmd:
        raise subcmd.TpmTestError(
            "Unexpected response subcommand" " field: %2.2x" % sub
        )
    if size != len(response):
        raise subcmd.TpmTestError(
            "Size mismatch: header %d, actual %d" % (size, len(response))
        )
    return response[header_size:]


def sb_GetDiceChain(tpm):
    print("Calling GetDiceChain")
    w_rsp = tpm.command(wrap_sb_command(SB_GetDiceChain, b""))
    return tpm.unwrap_ext_response(SB_GetDiceChain, w_rsp)


def sb_GenerateEcdsaP256KeyPair(tpm):
    print("Calling GenerateEcdsaP256KeyPair")
    w_rsp = tpm.command(wrap_sb_command(SB_RpcGenerateEcdsaP256KeyPair, b""))
    rsp = tpm.unwrap_ext_response(SB_RpcGenerateEcdsaP256KeyPair, w_rsp)
    # key blob's first word is number of the following words for the blob
    key_blob_words = int.from_bytes(rsp[:4], "little")
    key_blob = rsp[: key_blob_words * 4 + 4]
    # next the word with byte size of MAC'ed key comes
    maced_key_len = int.from_bytes(
        rsp[key_blob_words * 4 + 4 : key_blob_words * 4 + 8], "little"
    )
    # and the MAC'ed key itself
    maced_key = rsp[
        key_blob_words * 4 + 8 : key_blob_words * 4 + 8 + maced_key_len
    ]
    return key_blob, maced_key


def vc_SetStrongboxState(tpm, state: int):
    cmd = state.to_bytes(1, "little")
    print("vc_SetStrongboxState: ", cmd.hex())
    rsp = tpm.command(tpm.wrap_ext_command(subcmd.SET_STRONGBOX_STATE, cmd))
    return tpm.unwrap_ext_response(subcmd.SET_STRONGBOX_STATE, rsp)


def sb_SetHalBootInfo(
    tpm, os_version, os_patchlevel, vendor_patchlevel, boot_patchlevel
):
    cmd = (
        U32(os_version)
        + U32(os_patchlevel)
        + U32(vendor_patchlevel)
        + U32(boot_patchlevel)
    )
    print("sb_SetHalBootInfo:", cmd.hex())
    w_rsp = tpm.command(wrap_sb_command(SB_SetHalBootInfo, cmd))
    return tpm.unwrap_ext_response(SB_SetHalBootInfo, w_rsp)


# Adding size prefix for the byte string and pad to 32-bit aligned
def wrap_bytes(data):
    size = len(data)
    return U32(len(data)) + data + bytes((4 - size % 4) % 4)


def sb_GenerateCertificateV2Request(
    tpm, maced_keys: list, challenge, device_info
):
    cmd = (
        U32(len(maced_keys))
        + b"".join(wrap_bytes(key) for key in maced_keys)
        + wrap_bytes(challenge)
        + wrap_bytes(device_info)
    )
    print("GenerateCertificateV2Request:", cmd.hex())
    w_rsp = tpm.command(
        wrap_sb_command(SB_RpcGenerateCertificateV2Request, cmd)
    )
    csr = tpm.unwrap_ext_response(SB_RpcGenerateCertificateV2Request, w_rsp)
    return csr


# Set the default subject:
#  SEQUENCE 0x30 <size = 0x1f>
#   SET 0x31 <size = 0x1d>
#     SEQUENCE 0x30 <size = 0x1b>
#       OBJECT 0x06 <size = 0x03> 0x55 0x04 0x03 (:commonName)
#       PRINTABLESTRING 0x13 <size = 0x14> "Android Keystore Key"
DEFAULT_ISSUER = bytes.fromhex(
    "301f311d301b06035504031314416e64726f6964204b657973746f7265204b6579"
)


def sb_GenerateKey(tpm, key_params, attest_key, issuer_asn1=None):
    print("Calling GenerateKey")
    if not isinstance(key_params, list) or len(key_params) == 0:
        print("Expected list of key parameters")
        return None
    params = b"".join(key_params)
    issuer = DEFAULT_ISSUER if not issuer_asn1 else issuer_asn1
    g = U32(len(params) // 4) + params + attest_key + wrap_bytes(issuer)
    # attest key is currently ignored
    w_rsp = tpm.command(wrap_sb_command(SB_DeviceGenerateKey, g))
    rsp = tpm.unwrap_ext_response(SB_DeviceGenerateKey, w_rsp)
    key_blob_words = int.from_bytes(rsp[:4], "little")
    key_blob = rsp[: key_blob_words * 4 + 4]
    # Separating KeyCharacteristics from the blob.
    # word 0: total length of key blob in words
    # word 1: KM_SECURITY_STRONGBOX
    # word 2: length of hw params in words
    # .. hw enforced tags
    # word n: KM_SECURITY_KEYSTORE
    # word n+1: length of sw params in words
    # .. sw enforced tags
    hw_size = int.from_bytes(rsp[8:12], "little")
    sw_size = int.from_bytes(rsp[16 + hw_size * 4 : 20 + hw_size * 4], "little")

    if sw_size * 4 > len(key_blob):
        print("Incorrect size in blob:", sw_size, hex(sw_size))

    key_tags = key_blob[4 : 20 + (hw_size + sw_size) * 4]
    cert_size = int.from_bytes(
        rsp[key_blob_words * 4 + 4 : key_blob_words * 4 + 8], "little"
    )

    cert = rsp[key_blob_words * 4 + 8 : key_blob_words * 4 + 8 + cert_size]
    return key_blob, key_tags, cert


def sb_test(tpm):
    """Run TPM2 startup/shutdown in a loop tests"""
    tpm2_startup(tpm, TPM_SU_CLEAR)

    vc_SetStrongboxState(tpm, 1)

    w_rsp = tpm.command(wrap_sb_command(SB_DeviceGetHardwareInfo, b""))
    rsp = tpm.unwrap_ext_response(SB_DeviceGetHardwareInfo, w_rsp)
    print("Get Hardware Info:", rsp.hex())

    sb_SetHalBootInfo(tpm, 0x027100, 0x031710, 0x01350245, 0x013502450)

    dice_chain = sb_GetDiceChain(tpm)
    print("Received DICE chain:", dice_chain.hex())
    with open("dice_chain.cbor", "wb") as f:
        f.write(dice_chain)

    rkp_blob1, maced_key1 = sb_GenerateEcdsaP256KeyPair(tpm)
    print(f"RKP key blob[{len(rkp_blob1)}]={rkp_blob1.hex()}")

    # save without length prefix and alignment
    with open("maced_key.cbor", "wb") as f:
        f.write(maced_key1)
    cbor.verify_maced_key(maced_key1)

    rkp_blob2, maced_key2 = sb_GenerateEcdsaP256KeyPair(tpm)
    print(f"RKP key blob[{len(rkp_blob2)}]={rkp_blob2.hex()}")

    # save without length prefix and alignment
    with open("maced_key2.cbor", "wb") as f:
        f.write(maced_key2)
    cbor.verify_maced_key(maced_key2)

    # {"brand": "Google", "fused": 1, "model": "model", "device": "device",
    # "product": "Brya",  "os_version": "17", "manufacturer": "Google",
    # "vbmeta_digest": h'112233445566778899AABBCCDDEEFF',  "boot_patch_level": 20251026,
    # "system_patch_level": 20251025, "vendor_patch_level": 20251027,
    # "security_level": "strongbox", "vb_state": "green", "bootloader_state": "locked" }
    device_info = bytes.fromhex(
        "AE656272616E6466476F6F676C6565667573656401656D6F64656C656D6F64656C"
        "66646576696365666465766963656770726F647563746442727961"
        "6A6F735F76657273696F6E6231376C6D616E756661637475726572"
        "66476F6F676C656D76626D6574615F6469676573744F112233445566778899AABBCC"
        "DDEEFF70626F6F745F70617463685F6C6576656C1A013501927273797374656D5F70"
        "617463685F6C6576656C1A013501917276656E646F725F70617463685F6C6576656C"
        "1A01350193"
        "6E73656375726974795F6C6576656C697374726F6E67626F78"  # "security_level" : "strongbox"
        "6876625F737461746565677265656E"  # "vb_state":"green"
        "70626F6F746C6F616465725F7374617465666C6F636B6564"  # "bootloader_state":"locked"
    )
    csr = sb_GenerateCertificateV2Request(
        tpm, [], b"1234567890abcdefghijklmnopqrstuv", device_info
    )
    with open("csr0.cbor", "wb") as f:
        f.write(csr)
    cbor.verify_signed_data(csr)

    csr = sb_GenerateCertificateV2Request(
        tpm, [maced_key1], b"1234567890abcdefghijklmnopqrstuv", device_info
    )
    with open("csr1.cbor", "wb") as f:
        f.write(csr)
    cbor.verify_signed_data(csr)

    csr = sb_GenerateCertificateV2Request(
        tpm,
        [maced_key1, maced_key2],
        b"1234567890abcdefghijklmnopqrstuv",
        device_info,
    )
    with open("csr2.cbor", "wb") as f:
        f.write(csr)
    cbor.verify_signed_data(csr)

    # Test without attestation key
    key_blob, key_tags, cert = sb_GenerateKey(
        tpm,
        [
            Tag(KM_TAG_ALGORITHM, KM_ALG_EC),
            Tag(KM_TAG_KEY_SIZE, 256),
            Tag(KM_TAG_PURPOSE, KM_PURPOSE_SIGN),
            Tag(KM_TAG_DIGEST, KM_DIGEST_SHA_2_256),
            Tag(KM_TAG_DIGEST, KM_DIGEST_SHA_2_512),
            Tag(KM_TAG_ALLOW_WHILE_ON_BODY, 0),
            Tag(KM_TAG_USER_ID, 0xF00),
            Tag(KM_TAG_APPLICATION_ID, b"\xaa\xaa\xaa\xaa"),
            Tag(KM_TAG_APPLICATION_DATA, b"\xbb\xbb\xbb\xbb"),
        ],
        b"\x00\x00\x00\x00",
    )
    print(f"Key blob[{len(key_blob)}]={key_blob.hex()}")
    print(f"Key cert[{len(cert)}]={cert.hex()}")
    with open("cert_self.der", "wb") as f:
        f.write(cert)
    x509keymint.verify_certificate(cert)

    # Issuer: O=StrongBox, CN=Android Keystore Key
    issuer = bytes.fromhex(
        "303331123010060355040a0c095374726f6e67426f78311d301b06035504"
        "030c14416e64726f6964204b657973746f7265204b6579"
    )

    # Test with attestation key
    key_blob, key_tags, cert = sb_GenerateKey(
        tpm,
        [
            Tag(KM_TAG_ALGORITHM, KM_ALG_EC),
            Tag(KM_TAG_KEY_SIZE, 256),
            Tag(KM_TAG_PURPOSE, KM_PURPOSE_SIGN),
            Tag(KM_TAG_DIGEST, KM_DIGEST_SHA_2_256),
            Tag(KM_TAG_DIGEST, KM_DIGEST_SHA_2_512),
            Tag(KM_TAG_ALLOW_WHILE_ON_BODY, 0),
            Tag(KM_TAG_USER_ID, 0xF00),
            Tag(KM_TAG_CERTIFICATE_SUBJECT, DEFAULT_ISSUER),
            Tag(
                KM_TAG_CERTIFICATE_SERIAL,
                b"\x01\x23\x45\x67\x89\xab\xcd\xef01234567",
            ),
            Tag(KM_TAG_APPLICATION_ID, b"\xaa\xaa\xaa\xaa"),
            Tag(KM_TAG_APPLICATION_DATA, b"\xbb\xbb\xbb\xbb"),
            Tag(KM_TAG_NO_AUTH_REQUIRED, 0),
            Tag(KM_TAG_ATTESTATION_CHALLENGE, b"2025-11-05T19:57:14.294Z"),
            Tag(KM_TAG_ATTESTATION_APPLICATION_ID, b"com.google.android.gms"),
            Tag(KM_TAG_ATTESTATION_ID_BRAND, b"google"),
            Tag(KM_TAG_ATTESTATION_ID_DEVICE, b"brya"),
            Tag(KM_TAG_ATTESTATION_ID_PRODUCT, b"brya"),
            Tag(KM_TAG_ATTESTATION_ID_SERIAL, b"5CD5231RQK"),
            Tag(KM_TAG_ATTESTATION_ID_MANUFACTURER, b"Google"),
            Tag(KM_TAG_ATTESTATION_ID_MODEL, b"Brya"),
        ],
        rkp_blob1,
        issuer,
    )
    print(f"Key blob[{len(key_blob)}]={key_blob.hex()}")
    print(f"Key cert[{len(cert)}]={cert.hex()}")
    with open("cert.der", "wb") as f:
        f.write(cert)
    x509keymint.verify_certificate(cert)

    # expected blob header
    expected_tags = (
        # KeyCharacteristics
        U32(KM_SECURITY_STRONGBOX)
        + U32(21)
        + Tag(KM_TAG_ORIGIN, KM_ORIGIN_GENERATED)
        + Tag(KM_TAG_OS_VERSION, 0x027100)
        + Tag(KM_TAG_OS_PATCHLEVEL, 0x031710)
        + Tag(KM_TAG_VENDOR_PATCHLEVEL, 0x013502450)
        + Tag(KM_TAG_BOOT_PATCHLEVEL, 0x01350245)
        + Tag(KM_TAG_ALGORITHM, KM_ALG_EC)
        + Tag(KM_TAG_KEY_SIZE, 256)
        + Tag(KM_TAG_PURPOSE, KM_PURPOSE_SIGN)
        + Tag(KM_TAG_DIGEST, KM_DIGEST_SHA_2_256)
        + Tag(KM_TAG_DIGEST, KM_DIGEST_SHA_2_512)
        + Tag(KM_TAG_NO_AUTH_REQUIRED, 0)
        + U32(KM_SECURITY_KEYSTORE)
        + U32(3)
        + Tag(KM_TAG_ALLOW_WHILE_ON_BODY, 0)
        + Tag(KM_TAG_USER_ID, 0xF00)
    )

    if key_tags != expected_tags:
        print(
            f"KeyCharacteristics doesn't match:\nexpected: {expected_tags.hex()}, {len(expected_tags)} bytes\nreceived: {key_tags.hex()} {len(key_tags)} bytes"
        )

    print("Begin signing")
    # Additional parameters for blob authentication for begin()
    key_params = U32(8) + (
        Tag(KM_TAG_APPLICATION_ID, b"\xaa\xaa\xaa\xaa")
        + Tag(KM_TAG_APPLICATION_DATA, b"\xbb\xbb\xbb\xbb")
        + Tag(KM_TAG_DIGEST, KM_DIGEST_SHA_2_256)
        + Tag(KM_TAG_ALGORITHM, KM_ALG_EC)
    )
    begin_cmd = U32(KM_PURPOSE_SIGN) + key_blob + key_params
    w_rsp = tpm.command(wrap_sb_command(SB_DeviceBegin, begin_cmd))
    rsp = tpm.unwrap_ext_response(SB_DeviceBegin, w_rsp)
    print("r=", rsp.hex())
    operation_id = rsp[-4:]
    w_rsp = tpm.command(
        wrap_sb_command(
            SB_OperationUpdate,
            operation_id + U32(32) + b"0123456789ABCDEF0123456789ABCDEF",
        )
    )

    rsp = tpm.unwrap_ext_response(SB_OperationUpdate, w_rsp)
    print("r=", rsp.hex())

    w_rsp = tpm.command(
        wrap_sb_command(SB_OperationFinish, operation_id + U32(0))
    )
    rsp = tpm.unwrap_ext_response(SB_OperationFinish, w_rsp)
    print("r=", rsp.hex())

    vc_SetStrongboxState(tpm, 0)
    exception_detected = False
    try:
        vc_SetStrongboxState(tpm, 1)
    except subcmd.TpmTestError as tpm_exc:
        exception_detected = True

    if not exception_detected:
        print("Unexpected success in sb_SetStrongboxState")

    tpm2_shutdown(tpm, TPM_SU_STATE)

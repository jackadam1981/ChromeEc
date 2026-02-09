# -*- coding: utf-8 -*-
# Copyright 2026 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for validating CBOR MacedPublicKey, SignedData."""

import io
import struct


# ==========================================
# 1. Pure Python CBOR Parser
# ==========================================


class CBORDecodeError(Exception):
    pass


def loads(data):
    """Parses CBOR bytes into a Python object."""
    if isinstance(data, str):
        # Convenience: if hex string provided
        try:
            data = bytes.fromhex(data)
        except ValueError:
            pass

    stream = io.BytesIO(data)
    try:
        obj = _decode(stream)
        if stream.read(1):
            # Optional: warn about trailing bytes or ignore
            pass
        return obj
    except Exception as e:
        raise CBORDecodeError(f"Decoding failed: {e}")


def _decode(stream):
    byte = stream.read(1)
    if not byte:
        raise CBORDecodeError("Unexpected end of stream")

    b = byte[0]
    major_type = b >> 5
    additional_info = b & 0x1F

    # Read argument (length or value)
    if additional_info < 24:
        arg = additional_info
    elif additional_info == 24:
        arg = struct.unpack(">B", stream.read(1))[0]
    elif additional_info == 25:
        arg = struct.unpack(">H", stream.read(2))[0]
    elif additional_info == 26:
        arg = struct.unpack(">L", stream.read(4))[0]
    elif additional_info == 27:
        arg = struct.unpack(">Q", stream.read(8))[0]
    else:
        raise CBORDecodeError(f"Unsupported additional info: {additional_info}")

    if major_type == 0:  # Unsigned Int
        return arg
    elif major_type == 1:  # Negative Int
        return -1 - arg
    elif major_type == 2:  # Byte String
        return stream.read(arg)
    elif major_type == 3:  # Text String
        return stream.read(arg).decode("utf-8")
    elif major_type == 4:  # Array
        return [_decode(stream) for _ in range(arg)]
    elif major_type == 5:  # Map
        d = {}
        for _ in range(arg):
            k = _decode(stream)
            v = _decode(stream)
            if isinstance(k, (dict, list)):
                k = str(k)
            d[k] = v
        return d
    elif major_type == 6:  # Tag
        return _decode(stream)  # Recursively decode tagged content
    elif major_type == 7:  # Simple / Float
        if arg == 20:
            return False
        if arg == 21:
            return True
        if arg == 22:
            return None
        if additional_info == 26:
            return struct.unpack(">f", struct.pack(">L", arg))[0]
        if additional_info == 27:
            return struct.unpack(">d", struct.pack(">Q", arg))[0]

    raise CBORDecodeError(f"Unknown Major Type: {major_type}")


# ==========================================
# 2. CSR Verification (generateCertificateRequestV2)
# ==========================================
def verify_csr(data_bytes):
    """Verifies the nested structure of generateCertificateRequestV2."""
    print(f"\n[CSR] Verifying {len(data_bytes)} bytes...")

    try:
        # Top Level: [ version, UdsCerts, DiceCertChain, SignedData ]
        csr = loads(data_bytes)
    except CBORDecodeError as e:
        print(f"FAIL: Invalid CBOR - {e}")
        return False

    if not isinstance(csr, list) or len(csr) < 4:
        print("FAIL: CSR must be an Array of at least 4 items.")
        return False

    print(csr)
    version = csr[0]
    signed_data = csr[3]  # SignedData<Data>

    print(f"  OK: Outer Array found. Version: {version}")

    # SignedData is COSE_Sign1: [protected, unprotected, payload, signature]
    if not isinstance(signed_data, list) or len(signed_data) < 4:
        print("FAIL: SignedData is not a valid COSE_Sign1 Array.")
        return False

    # Payload is bstr .cbor Data
    sd_payload_bytes = signed_data[2]
    if not isinstance(sd_payload_bytes, bytes):
        print("FAIL: SignedData payload is not bytes.")
        return False

    # Unwrap: Data = [ challenge, bstr .cbor CsrPayload ]
    try:
        sd_payload = loads(sd_payload_bytes)
    except CBORDecodeError:
        print("FAIL: Could not decode SignedData payload.")
        return False

    if not isinstance(sd_payload, list) or len(sd_payload) < 2:
        print("FAIL: Data structure inside SignedData is incorrect.")
        return False

    print("  OK: SignedData payload structure matches.")

    # Unwrap: CsrPayload
    csr_payload_bytes = sd_payload[1]
    try:
        csr_payload = loads(csr_payload_bytes)
    except CBORDecodeError:
        print("FAIL: Could not decode inner CsrPayload.")
        return False

    # CsrPayload: [ version(3), CertificateType, DeviceInfo, KeysToSign ]
    if not isinstance(csr_payload, list) or len(csr_payload) < 4:
        print("FAIL: CsrPayload structure incorrect.")
        return False

    print(f"  OK: CsrPayload decoded (Version {csr_payload[0]}).")
    print(f"  SUCCESS: CSR structure valid.")
    return True


def verify_signed_data(data_bytes):
    """
    Verifies a standalone SignedData structure (COSE_Sign1).
    Schema:
      SignedData<Data> = [
        protected: bstr .cbor { 1 : Alg },
        unprotected: {},
        payload: bstr .cbor Data,
        signature: bstr
      ]
      Data = [ challenge: bstr, bstr .cbor T ]
    """
    print(f"\n[SignedData] Verifying {len(data_bytes)} bytes...")

    try:
        # 1. Decode the COSE_Sign1 Array
        cose_obj = loads(data_bytes)
    except CBORDecodeError:
        print("FAIL: Invalid CBOR.")
        return False

    if not isinstance(cose_obj, list) or len(cose_obj) < 4:
        print(
            "FAIL: SignedData must be an Array of 4 items [prot, unprot, payload, sig]."
        )
        return False

    # --- Field 0: Protected Headers ---
    protected_bytes = cose_obj[0]
    if not isinstance(protected_bytes, bytes):
        print("FAIL: Protected header must be bytes.")
        return False

    # Optional: Verify Algorithm inside protected header
    try:
        protected_map = loads(protected_bytes)
        # Label 1 is Algorithm
        if 1 in protected_map:
            alg = protected_map[1]
            # Valid Algs: EdDSA(-8), ES256(-7), ES384(-35)
            if alg in [-8, -7, -35]:
                print(f"  OK: Protected Header (Alg={alg})")
            else:
                print(f"  WARN: Unknown Algorithm in protected header: {alg}")
    except CBORDecodeError:
        print("FAIL: Protected header contains invalid CBOR.")
        return False

    # --- Field 2: Payload ---
    payload_bytes = cose_obj[2]
    if not isinstance(payload_bytes, bytes):
        print("FAIL: Payload must be bytes (bstr).")
        return False

    # --- Field 3: Signature ---
    signature = cose_obj[3]
    if not isinstance(signature, bytes):
        print("FAIL: Signature must be bytes.")
        return False
    print(f"  OK: Signature found ({len(signature)} bytes).")

    # --- Inner Payload Verification ---
    # The payload is 'bstr .cbor Data'
    # Data is [ challenge, bstr .cbor T ]
    print("  -> Decoding Payload...")
    try:
        data_arr = loads(payload_bytes)
    except CBORDecodeError:
        print("FAIL: Payload is not valid CBOR.")
        return False

    if not isinstance(data_arr, list) or len(data_arr) < 2:
        print("FAIL: Payload Data must be an array [challenge, wrapped_obj].")
        return False

    challenge = data_arr[0]
    wrapped_obj = data_arr[1]

    if not isinstance(challenge, bytes):
        print("FAIL: Challenge (item 0) must be bytes.")
        return False

    # Check challenge size (0..64 as per CDDL)
    if len(challenge) > 64:
        print(f"FAIL: Challenge too large ({len(challenge)} bytes). Max is 64.")
        return False

    if not isinstance(wrapped_obj, bytes):
        print("FAIL: Wrapped Object (item 1) must be bytes (bstr .cbor T).")
        return False

    print(f"  SUCCESS: SignedData valid.")
    print(f"     - Challenge: {len(challenge)} bytes")
    print(f"     - Inner Object: {len(wrapped_obj)} bytes")

    return True


# ==========================================
# 3. MacedPublicKey Verification
# ==========================================


def verify_public_key(key_obj):
    """Validates the COSE Key Map structure."""
    if not isinstance(key_obj, dict):
        print("FAIL: PublicKey is not a Map.")
        return False

    # COSE Constants
    KTY_OKP, KTY_EC2 = 1, 2
    ALG_ES256, ALG_ES384, ALG_EDDSA = -7, -35, -8
    CRV_P256, CRV_P384, CRV_ED25519 = 1, 2, 6
    LABEL_KTY, LABEL_ALG, LABEL_CRV = 1, 3, -1

    kty = key_obj.get(LABEL_KTY)
    alg = key_obj.get(LABEL_ALG)
    crv = key_obj.get(LABEL_CRV)

    if kty == KTY_OKP:
        if alg == ALG_EDDSA and crv == CRV_ED25519:
            print("    OK: Valid Ed25519 Key")
            return True
    elif kty == KTY_EC2:
        if (alg == ALG_ES256 and crv == CRV_P256) or (
            alg == ALG_ES384 and crv == CRV_P384
        ):
            print(f"    OK: Valid ECDSA Key (Alg: {alg})")
            return True

    print(f"FAIL: Invalid Key combination (Kty: {kty}, Alg: {alg}, Crv: {crv})")
    return False


def verify_maced_key(data_bytes):
    """Verifies MacedPublicKey structure."""
    print(f"\n[MacedKey] Verifying {len(data_bytes)} bytes...")

    try:
        # MacedPublicKey is COSE_Mac0: [protected, unprotected, payload, tag]
        mpk = loads(data_bytes)
    except CBORDecodeError:
        print("FAIL: Invalid CBOR.")
        return False

    if not isinstance(mpk, list) or len(mpk) < 4:
        print("FAIL: MacedPublicKey must be an Array of 4 items.")
        return False

    payload_bytes = mpk[2]  # bstr .cbor PublicKey
    if not isinstance(payload_bytes, bytes):
        print("FAIL: Payload is not bytes.")
        return False

    print("  OK: COSE_Mac0 structure found.")

    try:
        public_key = loads(payload_bytes)
    except CBORDecodeError:
        print("FAIL: Could not decode inner PublicKey.")
        return False

    if verify_public_key(public_key):
        print("  SUCCESS: MacedPublicKey structure valid.")
        return True
    return False

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2026 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for parsing X509 KeyMint extension"""

import io
import sys


# --- 1. Minimal ASN.1 Parser (Zero Dependencies) ---
class ASN1ParseError(Exception):
    pass


class SimpleASN1Parser:
    def __init__(self, data):
        self.stream = (
            io.BytesIO(data)
            if isinstance(data, bytes)
            else io.BytesIO(bytes.fromhex(data))
        )

    def is_empty(self):
        current = self.stream.tell()
        byte = self.stream.read(1)
        self.stream.seek(current)
        return not byte

    def peek_tag(self):
        pos = self.stream.tell()
        byte = self.stream.read(1)
        self.stream.seek(pos)
        if not byte:
            return None
        return byte[0]

    def read_tlv(self):
        """
        Reads a TLV triplet.
        Returns: (tag_number, is_constructed, value_bytes)
        """
        # --- 1. Read Tag ---
        byte = self.stream.read(1)
        if not byte:
            return None, None, None
        b = byte[0]

        is_constructed = bool(b & 0x20)
        tag_number = b & 0x1F

        # Handle High-Tag-Number form (if tag_number == 31)
        if tag_number == 0x1F:
            tag_number = 0
            while True:
                next_byte = self.stream.read(1)
                if not next_byte:
                    raise ASN1ParseError("Unexpected EOF in High Tag")
                nb = next_byte[0]
                tag_number = (tag_number << 7) | (nb & 0x7F)
                if not (nb & 0x80):
                    break

        # --- 2. Read Length ---
        len_byte = self.stream.read(1)
        if not len_byte:
            raise ASN1ParseError("Unexpected EOF reading length")
        length = len_byte[0]

        if length & 0x80:
            num_bytes = length & 0x7F
            if num_bytes == 0:
                raise ASN1ParseError("Indefinite length not supported")
            len_bytes = self.stream.read(num_bytes)
            length = int.from_bytes(len_bytes, byteorder="big")

        # --- 3. Read Value ---
        value = self.stream.read(length)
        if len(value) != length:
            raise ASN1ParseError("Truncated value")

        return tag_number, is_constructed, value

    def read_sequence_content(self):
        """Consumes a SEQUENCE header and returns a parser for its content."""
        tag, constructed, val = self.read_tlv()
        if tag != 0x10:  # 0x10 is Tag 16 (SEQUENCE)
            # Note: Some structures use implicit tagging, but standard X.509/KeyMint uses explicit SEQUENCE
            # We allow a fallback check if it's constructed
            if not constructed:
                raise ASN1ParseError(f"Expected SEQUENCE (16), got {tag}")
        return SimpleASN1Parser(val)

    def read_oid(self):
        """Reads an OID and returns it as a dot-notation string."""
        tag, length, value = self.read_tlv()
        if tag != 0x06:
            raise ASN1ParseError(f"Expected OID (0x06), got {hex(tag)}")

        # Decode OID
        res = []
        val = 0
        first_byte = value[0]
        res.append(str(first_byte // 40))
        res.append(str(first_byte % 40))

        val = 0
        for byte in value[1:]:
            if byte < 128:
                val = val * 128 + byte
                res.append(str(val))
                val = 0
            else:
                val = val * 128 + (byte & 0x7F)
        return ".".join(res)

    def read_int(self):
        tag, const, value = self.read_tlv()
        if tag != 0x02:
            raise ASN1ParseError(f"Expected INTEGER (0x02), got {hex(tag)}")
        return int.from_bytes(value, byteorder="big")

    def read_enum(self):
        tag, const, value = self.read_tlv()
        if tag != 0x0A:
            raise ASN1ParseError(f"Expected ENUMERATED (0x0A), got {hex(tag)}")
        return int.from_bytes(value, byteorder="big")

    def read_octet_string(self):
        tag, const, value = self.read_tlv()
        if tag != 0x04:
            raise ASN1ParseError(
                f"Expected OCTET STRING (0x04), got {hex(tag)}"
            )
        return value


# --- 2. KeyMint Extension Parser ---
# Mapping of KeyMint Tag IDs to Names
KM_TAGS = {
    1: "Purpose",
    2: "Algorithm",
    3: "KeySize",
    5: "Digest",
    6: "Padding",
    7: "CallerNonce",
    10: "EcCurve",
    200: "RsaPublicExponent",
    301: "ActiveDateTime",
    302: "OriginationExpireDateTime",
    400: "UsageExpireDateTime",
    401: "MinSecondsBetweenOps",
    402: "MaxUsesPerBoot",
    503: "NoAuthRequired",
    504: "UserAuthType",
    505: "AuthTimeout",
    506: "AllowWhileOnBody",
    600: "AllApplications",
    601: "ApplicationId",
    701: "CreationDateTime",
    702: "Origin",
    704: "RootOfTrust",
    705: "OsVersion",
    706: "OsPatchLevel",
    709: "AttestationChallenge",
    710: "AttestationApplicationId",
    711: "AttestationIdBrand",
    712: "AttestationIdDevice",
    713: "AttestationIdProduct",
    714: "AttestationIdSerial",
    715: "AttestationIdImei",
    716: "AttestationIdMeid",
    717: "AttestationIdManufacturer",
    718: "AttestationIdModel",
}


def parse_authorization_list(auth_data, label="AuthorizationList"):
    """
    Parses the Software/Hardware Enforced Authorization List.
    Structure: SEQUENCE of [TagNo] EXPLICIT Value
    """
    print(f"  [{label}] Parsing {len(auth_data)} bytes...")
    p = SimpleASN1Parser(auth_data)

    while not p.is_empty():
        # 1. Read the Outer Tag (The Context Specific Tag ID)
        tag_id, constructed, wrapped_val = p.read_tlv()

        if tag_id is None:
            break

        tag_name = KM_TAGS.get(tag_id, f"UnknownTag({tag_id})")

        # 2. Unwrap the Explicit Value
        # Because KeyMint uses EXPLICIT tagging, wrapped_val contains the actual ASN.1 object (TLV)
        inner_p = SimpleASN1Parser(wrapped_val)
        inner_tag, inner_const, inner_val = inner_p.read_tlv()

        # 3. Format Output based on Tag ID or Inner Type
        display_val = "..."

        # Handle RootOfTrust (704) specifically as it is a nested Sequence
        if tag_id == 704:
            display_val = f"SEQUENCE ({len(inner_val)} bytes)"
            # Optional: Recursively parse RootOfTrust fields here if needed
        elif inner_tag == 0x02:  # INTEGER
            display_val = int.from_bytes(inner_val, byteorder="big")
        elif inner_tag == 0x04:  # OCTET STRING
            display_val = f"<{len(inner_val)} bytes> {inner_val.hex()}..."
        elif inner_tag == 0x01:  # BOOLEAN
            display_val = "True" if inner_val != b"\x00" else "False"
        elif (
            inner_tag == 0x31 or inner_tag == 0x11
        ):  # SET (e.g. Purpose, Digest)
            # Sets usually contain integers in KeyMint
            try:
                set_p = SimpleASN1Parser(inner_val)
                items = []
                while not set_p.is_empty():
                    _, _, v = set_p.read_tlv()
                    items.append(int.from_bytes(v, byteorder="big"))
                display_val = f"SET {items}"
            except:
                display_val = "SET <complex>"
        elif inner_tag == 0x05:  # NULL
            display_val = "Present"
        else:
            display_val = f"Raw: {inner_val.hex()}..."

        print(f"    - {tag_id} {tag_name}: {display_val}")


def parse_keymint_extension(ext_data):
    """Parses the KeyMint extension value (inside the wrapping OCTET STRING)."""
    print(f"\n[KeyMint] Parsing extension content ({len(ext_data)} bytes)...")
    p = SimpleASN1Parser(ext_data)

    try:
        # KeyDescription SEQUENCE
        seq = p.read_sequence_content()

        # 1. Attestation Version
        att_ver = seq.read_int()
        print(f"  - Attestation Version: {att_ver}")

        # 2. Attestation Security Level
        sec_lvl = seq.read_enum()
        lvl_map = {0: "Software", 1: "TEE", 2: "StrongBox"}
        print(
            f"  - Attestation Security Level: {lvl_map.get(sec_lvl, sec_lvl)}"
        )

        # 3. KeyMint Version
        km_ver = seq.read_int()
        print(f"  - KeyMint Version: {km_ver}")

        # 4. KeyMint Security Level
        km_lvl = seq.read_enum()
        print(f"  - KeyMint Security Level: {lvl_map.get(km_lvl, km_lvl)}")

        # 5. Challenge
        challenge = seq.read_octet_string()
        print(
            f"  - Attestation Challenge: {len(challenge)} bytes, {challenge.hex()}"
        )

        # 6. Unique ID
        uid = seq.read_octet_string()
        print(f"  - Unique ID: {len(uid)} bytes, {uid.hex()}")

        # 7. Software Enforced (SEQUENCE)
        _, _, sw_bytes = seq.read_tlv()  # Read SEQUENCE blob
        parse_authorization_list(sw_bytes, "SoftwareEnforced")

        # 8. Hardware Enforced (SEQUENCE)
        _, _, hw_bytes = seq.read_tlv()  # Read SEQUENCE blob
        parse_authorization_list(hw_bytes, "HardwareEnforced")

        print("\nSUCCESS: KeyMint Extension structure verified.")
        return True

    except Exception as e:
        print(f"  FAIL: KeyMint parsing error: {e}")
        return False


# --- 3. Main X.509 Scanner ---


def verify_certificate(cert_input):
    print("--- X.509 Verification ---")

    # Handle input: Strings are treated as Hex, Bytes are treated as raw DER
    if isinstance(cert_input, str):
        # Remove potential whitespace/newlines from hex string
        cert_input = "".join(cert_input.split())
        data = bytes.fromhex(cert_input)
    else:
        data = cert_input

    p = SimpleASN1Parser(data)

    try:
        # 1. Certificate is a SEQUENCE
        cert_seq = p.read_sequence_content()

        # 2. TBSCertificate is the first element (SEQUENCE)
        tbs_seq_data = cert_seq.read_tlv()[2]  # Get raw bytes of TBS
        tbs = SimpleASN1Parser(tbs_seq_data)

        # Inside TBS:
        # [0] Version (Explicit Tag 0) - Optional, but usually present
        if tbs.peek_tag() == 0xA0:
            tbs.read_tlv()  # Skip Version

        tbs.read_tlv()  # Serial Number (INTEGER)
        tbs.read_tlv()  # Signature Algo (SEQUENCE)
        tbs.read_tlv()  # Issuer (SEQUENCE)
        tbs.read_tlv()  # Validity (SEQUENCE)
        tbs.read_tlv()  # Subject (SEQUENCE)
        tbs.read_tlv()  # SubjectPublicKeyInfo (SEQUENCE)

        # Skip optional [1] IssuerUniqueID or [2] SubjectUniqueID if present
        while True:
            # Check for end of stream or next tag
            if tbs.is_empty():
                break

            tag = tbs.peek_tag()
            if tag == 0xA3:  # [3] Extensions (Explicit Tag)
                break
            # Skip whatever else is here
            tbs.read_tlv()

        # Parse Extensions Wrapper
        ext_tag, _, ext_wrapper_val = tbs.read_tlv()
        if ext_tag != 0x3:
            print("FAIL: Extensions not found in TBS.", ext_tag)
            return

        # Parse the Sequence of Extensions
        ext_seq = SimpleASN1Parser(ext_wrapper_val).read_sequence_content()

        # Iterate through extensions looking for Android OID
        ANDROID_OID = "1.3.6.1.4.1.11129.2.1.17"
        found = False

        while True:
            # Each Extension is a SEQUENCE
            ext_tag, ext_len, ext_val = ext_seq.read_tlv()
            if not ext_tag:
                break  # End of extensions

            ext_p = SimpleASN1Parser(ext_val)
            oid = ext_p.read_oid()

            # Critical flag (BOOLEAN) is optional
            if ext_p.peek_tag() == 0x01:
                ext_p.read_tlv()

            # Extension Value is OCTET STRING
            val_tag, val_len, val_bytes = ext_p.read_tlv()

            if oid == ANDROID_OID:
                print(f"FOUND: Android KeyMint Extension (OID {oid})")
                parse_keymint_extension(val_bytes)
                found = True
                break

        if not found:
            print("FAIL: Android KeyMint Extension OID not found.")

    except Exception as e:
        print(f"FAIL: X.509 Parsing Error: {e}")
        import traceback

        traceback.print_exc()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
        try:
            with open(file_path, "rb") as f:
                cert_data = f.read()
                verify_certificate(cert_data)
        except FileNotFoundError:
            print(f"Error: File '{file_path}' not found.")
        except Exception as e:
            print(f"Error reading file: {e}")
    else:
        print("Usage: python3 verify_attestation.py <path_to_der_cert>")

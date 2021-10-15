# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for testing ecc functions using extended commands."""
from binascii import a2b_hex as a2b
import hashlib
import os
import struct

import subcmd
import utils
from pyasn1.type import univ
from pyasn1.codec.ber import encoder, decoder

_ECC_OPCODES = {
  'SIGN': 0x00,
  'VERIFY': 0x01,
  'KEYGEN': 0x02,
  'KEYDERIVE': 0x03,
  'TEST_POINT': 0x04,
  'VERIFY_ANY': 0x05,
  'SIGN_ANY': 0x06,
}

_ECC_CURVES = {
  'NIST-P256': 0x03,
}

# TPM2 signature codes.
_SIGN_MODE = {
  'NONE': 0x00,
  'ECDSA': 0x18
}

# TPM2 ALG codes.
_HASH = {
  'NONE': 0x10,
  'SHA1': 0x04,
  'SHA256': 0x0B
}

_HASH_FUNC = {
  'NIST-P256': hashlib.sha256
}

#
# Field size:
# FIELD          LENGTH
# OP             1
# CURVE_ID       1
# SIGN_MODE      1
# HASHING        1
# MSG_LEN        2 (big endian)
# MSG            MSG_LEN
# R_LEN          2 (big endian)
# R              R_LEN
# S_LEN          2 (big endian)
# S              S_LEN
# DIGEST_LEN     2 (big endian)
# DIGEST         DIGEST_LEN
# D_LEN          2 (big endian)
# D              D_LEN
# QX_LEN         2 (big endian)
# QX             QX_LEN
# QY_LEN         2 (big endian)
# QY             QX_LEN
#
# Command formats:

# TEST_SIGN:
# OP | CURVE_ID | SIGN_MODE | HASHING | DIGEST_LEN | DIGEST
#    @returns 0/1 | R_LEN | R | S_LEN | S
def _sign_cmd(curve_id, hash_func, sign_mode, msg):
    digest = hash_func(msg).digest()
    return struct.pack('>BBBBH', _ECC_OPCODES['SIGN'], curve_id, sign_mode,
                      _HASH['NONE'], len(digest)) + digest

# TEST_VERIFY:
# OP | CURVE_ID | SIGN_MODE | HASHING | R_LEN | R | S_LEN | S
#   DIGEST_LEN | DIGEST
#    @returns 1 if successful
# below we assume sig = [R_LEN | R | S_LEN | S] as it came from SIGN
def _verify_cmd(curve_id, hash_func, sign_mode, msg, sig):
    digest = hash_func(msg).digest()
    return struct.pack('>BBBB', _ECC_OPCODES['VERIFY'], curve_id, sign_mode,
                      _HASH['NONE']) + sig +\
                      len(digest).to_bytes(2, 'big') + digest

# TEST_SIGN_ANY:
# OP | CURVE_ID | SIGN_MODE | HASHING | DIGEST_LEN | DIGEST | D_LEN | D
#    @returns 0/1 | R_LEN | R | S_LEN | S
def _sign_any_cmd(curve_id, hash_func, sign_mode, msg, pkey):
    digest = hash_func(msg).digest()
    return struct.pack('>BBBBH', _ECC_OPCODES['SIGN_ANY'], curve_id, sign_mode,
                      _HASH['NONE'], len(digest)) + digest +\
                        len(pkey).to_bytes(2, 'big') + pkey

# TEST_VERIFY_ANY:
# OP | CURVE_ID | SIGN_MODE | HASHING | R_LEN | R | S_LEN | S |
#   DIGEST_LEN | DIGEST | QX_LEN | QX | QY_LEN | QY
#    @returns 1 if successful
# pylint: disable=too-many-arguments
def _verify_any_cmd(curve_id, hash_func, sign_mode, msg, sig, q_x, q_y):
    digest = hash_func(msg).digest()
    return struct.pack('>BBBB', _ECC_OPCODES['VERIFY_ANY'], curve_id, sign_mode,
                      _HASH['NONE']) + sig +\
                      len(digest).to_bytes(2, 'big') + digest +\
                      len(q_x).to_bytes(2, 'big') + q_x+ \
                      len(q_y).to_bytes(2, 'big') + q_y

# TEST_POINT:
# OP | CURVE_ID | QX_LEN | QX | QY_LEN | QY
#    @returns 1 if point is on curve
def _test_point_cmd(curve_id, q_x, q_y):
    return struct.pack('>BB', _ECC_OPCODES['TEST_POINT'], curve_id) +\
          len(q_x).to_bytes(2, 'big') + q_x+ \
          len(q_y).to_bytes(2, 'big') + q_y

#
# TEST_KEYGEN:
# OP | CURVE_ID
#    @returns 0/1 | D_LEN | D | QX_LEN | QX | QY_LEN | QY
def _keygen_cmd(curve_id):
    return struct.pack('>BB', _ECC_OPCODES['KEYGEN'], curve_id)

# TEST_KEYDERIVE:
# OP | CURVE_ID | SEED_LEN | SEED
#    @returns 1 if successful
def _keyderive_cmd(curve_id, seed):
    return struct.pack('>BBH', _ECC_OPCODES['KEYDERIVE'], curve_id,
                       len(seed)) + seed

def int_to_tpm2b(i: int):
    bits = i.bit_length()
    bytes = (bits + 7) // 8
    return bytes.to_bytes(2, 'big') + i.to_bytes(bytes, 'big')

def ecdsa_verify_test(tpm, filename: str, skip_tests: list = []):
    """Run ECDSA Verify tests"""
    test_inputs = utils.read_vectors(filename)
    if test_inputs['algorithm'] != 'ECDSA':
        return
    passed = 0
    failed = 0
    print(test_inputs['header'])
    for test_group in test_inputs['testGroups']:
        if test_group['sha'] != 'SHA-256':
            continue
        key = test_group['key']
        if key['curve'] != 'secp256r1':
            continue
        print(test_group['type'])
        key_x = bytes.fromhex(key['wx'])
        key_y = bytes.fromhex(key['wy'])
        for test in test_group['tests']:
            tcId = test['tcId']
            test_name = 'Test '+ str(tcId) + ' ' + test['comment']
            msg = bytes.fromhex(test['msg'])
            sig_asn1 = bytes.fromhex(test['sig'])
            try:
                sig_d = decoder.decode(sig_asn1)
                sig_r = int_to_tpm2b(sig_d[0][0].__int__())
                sig_s = int_to_tpm2b(sig_d[0][1].__int__())
            except:
                continue
            if tcId in skip_tests:
                continue
            sig = sig_r + sig_s
            print(test_name, end='')
            cmd = _verify_any_cmd(_ECC_CURVES['NIST-P256'],
                                hashlib.sha256,
                                _SIGN_MODE['ECDSA'], msg, sig,
                                key_x, key_y)
            if test['result'] in ['valid', 'acceptable']:
                expected = b'\x01'
            else:
                expected = b'\x00'
            response = tpm.command(tpm.wrap_ext_command(subcmd.ECC, cmd))
            verified = tpm.unwrap_ext_response(subcmd.ECC, response)
            if verified[:1] != expected:
                print(' FAILED')
                failed += 1
            else:
                print(' PASSED')
                passed += 1
    print('Passed', passed, 'Failed', failed)

def ecdsa_verify_nist_test(tpm, inputs: str, results: str):
    """Run ECDSA Verify tests"""
    test_inputs = utils.read_vectors(inputs)
    test_results = utils.read_vectors(results)
    if test_inputs['algorithm'] != 'ECDSA':
        return
    passed = 0
    failed = 0
    for test_group in test_inputs['testGroups']:
        if test_group['hashAlg'] != 'SHA2-256':
            continue
        if test_group['curve'] != 'P-256':
            continue
        tgId = test_group['tgId']
        tg_results = [x['tests'] for x in test_results if x['tgId']==tgId]

        for test in test_group['tests']:
            tcId = test['tcId']
            test_name = 'Test '+ str(tcId)
            test_result = [x['testPassed'] for x in tg_results[0] if x['tcId']==tcId]
            msg = bytes.fromhex(test['message'])
            key_x = bytes.fromhex(test['qx'])
            key_y = bytes.fromhex(test['qy'])
            sig_r = int_to_tpm2b(int(test['r'], 16))
            sig_s = int_to_tpm2b(int(test['s'], 16))
            sig = sig_r + sig_s
            print(test_name, end='')
            cmd = _verify_any_cmd(_ECC_CURVES['NIST-P256'],
                                hashlib.sha256,
                                _SIGN_MODE['ECDSA'], msg, sig,
                                key_x, key_y)
            if test_result == True:
                expected = b'\x01'
            else:
                expected = b'\x00'
            response = tpm.command(tpm.wrap_ext_command(subcmd.ECC, cmd))
            verified = tpm.unwrap_ext_response(subcmd.ECC, response)
            if verified[:1] != expected:
                print(' FAILED')
                failed += 1
            else:
                print(' PASSED')
                passed += 1
    print('Passed', passed, 'Failed', failed)

def ecc_test(tpm):
    nist_test_vector = "test_vectors/ecdsa_nist_sigver_test_1.json"
    nist_test_expected = "test_vectors/ecdsa_nist_sigver_expected_1.json"
    ecdsa_verify_nist_test(tpm, nist_test_vector, nist_test_expected)

    # ecdsa_verify_file = "test_vectors/ecdsa_secp256r1_sha256_test.json"
    # # specific tests to skip as they fail due to not very strict ASN.1 parser
    # ecdsa_verify_skip_list = [4, 5, 15, 22, 23, 43, 49, 56, 58, 60, 63, 67,
    #                           68, 69, 70, 71, 95, 96, 111]
    # ecdsa_verify_test(tpm, ecdsa_verify_file, ecdsa_verify_skip_list)

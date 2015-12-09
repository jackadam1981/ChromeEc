#!/usr/bin/python
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for testing rsa functions using extended commands."""

import sha
import struct

import subcmd


_RSA_OPCODES = {
    'ENCRYPT': 0,
    'DECRYPT': 1,
    'SIGN': 2,
    'VERIFY': 3,
    'KEYGEN': 4
}


# TPM2 ALG codes.
_RSA_PADDING = {
    'PKCS1-SSA': 0x14,
    'PKCS1-ES': 0x15,
    'OAEP': 0x17
}


# TPM2 ALG codes.
_HASH = {
    'SHA1': 4,
    'SHA256': 0x0B
}


# Command format.
#
#   0x00 OP
#   0x00 PADDING
#   0x00 HASHING
#   0x00 MSB KEY LEN
#   0x00 LSB KEY LEN
#   0x00 MSB IN LEN
#   0x00 LSB IN LEN
#   .... IN
#   0x00 MSB DIGEST LEN
#   0x00 LSB DIGEST LEN
#   .... DIGEST
#
_RSA_CMD_FORMAT = '{o:c}{p:c}{h:c}{kl:s}{ml:s}{msg}{dl:s}{dig}'


def _decrypt_cmd(padding, hashing, key_len, msg):
    op = _RSA_OPCODES['DECRYPT']
    msg_len = len(msg)
    return _RSA_CMD_FORMAT.format(o=op, p=padding, h=hashing,
                                  kl=struct.pack('>H', key_len),
                                  ml=struct.pack('>H', msg_len), msg=msg,
                                  dl='', dig='')


def _encrypt_cmd(padding, hashing, key_len, msg):
    op = _RSA_OPCODES['ENCRYPT']
    msg_len = len(msg)
    return _RSA_CMD_FORMAT.format(o=op, p=padding, h=hashing,
                                  kl=struct.pack('>H', key_len),
                                  ml=struct.pack('>H', msg_len), msg=msg,
                                  dl='', dig='')


def _sign_cmd(padding, hashing, key_len, msg):
    op = _RSA_OPCODES['SIGN']
    digest = sha.sha(msg).digest()
    digest_len = len(digest)
    return _RSA_CMD_FORMAT.format(o=op, p=padding, h=hashing,
                                  kl=struct.pack('>H', key_len),
                                  ml=struct.pack('>H', digest_len), msg=digest,
                                  dl='', dig='')


def _verify_cmd(padding, hashing, key_len, sig, msg):
    op = _RSA_OPCODES['VERIFY']
    sig_len = len(sig)
    digest = sha.sha(msg).digest()
    digest_len = len(digest)
    return _RSA_CMD_FORMAT.format(o=op, p=padding, h=hashing,
                                  kl=struct.pack('>H', key_len),
                                  ml=struct.pack('>H', sig_len), msg=sig,
                                  dl=struct.pack('>H', digest_len), dig=digest)


#
# TEST VECTORS.
#
_ENCRYPT_INPUTS = (
    (_RSA_PADDING['OAEP'], _HASH['SHA1'], 768),
    (_RSA_PADDING['OAEP'], _HASH['SHA256'], 768),
    (_RSA_PADDING['PKCS1-ES'], 0, 768),
    # TODO: investigate, 2048 causes a crash in uart after completion.
    #(_RSA_PADDING['PKCS1-ES'], 0, 2048),
)


_SIGN_INPUTS = (
    # TODO: add support for PSS & SHA256.
    (_RSA_PADDING['PKCS1-SSA'], _HASH['SHA1'], 768),
)


def _encrypt_tests(tpm):
    msg = 'Hello CR50!'

    for data in _ENCRYPT_INPUTS:
        padding, hashing, key_len = data
        cmd = _encrypt_cmd(padding, hashing, key_len, msg)
        wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.RSA, cmd))
        ciphertext = tpm.unwrap_ext_response(subcmd.RSA, wrapped_response)

        cmd = _decrypt_cmd(padding, hashing, key_len, ciphertext)
        wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.RSA, cmd))
        plaintext = tpm.unwrap_ext_response(subcmd.RSA, wrapped_response)
        if msg != plaintext:
            print 'ENCRYPT TESTS FAILURE: %s' % data
            return
    print 'ENCRYPT TESTS SUCCESS'


def _sign_tests(tpm):
    msg = 'Hello CR50!'

    for data in _SIGN_INPUTS:
        padding, hashing, key_len = data
        cmd = _sign_cmd(padding, hashing, key_len, msg)
        wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.RSA, cmd))
        signature = tpm.unwrap_ext_response(subcmd.RSA, wrapped_response)

        with open('/tmp/s', 'w') as f:
            f.write(signature)

        cmd = _verify_cmd(padding, hashing, key_len, signature, msg)
        wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.RSA, cmd))
        verified = tpm.unwrap_ext_response(subcmd.RSA, wrapped_response)
        if verified != '\x01':
            print 'SIGN TESTS FAILURE: %s' % data
            return
    print 'SIGN TESTS SUCCESS'


def rsa_test(tpm):
    _encrypt_tests(tpm)
    _sign_tests(tpm)


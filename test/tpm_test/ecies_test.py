#!/usr/bin/env python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for testing ECIES using extended commands."""
from binascii import b2a_hex as b2a
from binascii import a2b_hex as a2b
from struct import pack

import subcmd
import utils

_ECIES_OPCODES = {
  'ENCRYPT': 0x00,
  'DECRYPT': 0x01,
}



_DEFAULT_SALT = b'Salt!'
_DEFAULT_INFO = b'Info!'
_STATIC_IV =  bytes([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])

_ECIES_INPUTS = (
  (
    b'',
    b'Test message!!',
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'SIMPLE'
  ),
  (
    b'',
    b'Multi block test message!!!!',
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'MULTI-BLOCK'
  ),
  (
    b'Auth data',
    b'Test message!!!!',
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'AUTH-DATA'
  ),
  (
    b'Auth data' * 10,
    b'Test message!!!!',
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'LARGE-AUTH-DATA'
  ),
  (
    b'Auth data',
    b'Test message!!!!' * 5,
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'LARGE-PLAINTEXT-DATA'
  ),
  (
    b'',
    b'Test message!!',
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    b'',
    b'',
    'NO-SALT-INFO'
  ),
  (
    b'Auth data',
    b'',
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'AUTH-NULL-PLAINTEXT'
  ),
  (
    b'',
    b'',
    _STATIC_IV,
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    a2b('98e76f53febd6bedc8fa19ce1543cb3f8f5cbc72c74602f1bfdee88c19d3d9d0'),
    a2b('8750c295cd33be5846868e2869bf2c8cfeefbc4a574874c7388bf40f74e8e0e6'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'NULL-PLAINTEXT'
  ),
)

_ECIES_COMPAT_INPUTS = (
  (
    a2b('d61262f22e8c70414777cbc060d1e387'),
    b'The quick brown fox jumps over the lazy dog.',
    a2b('d61262f22e8c70414777cbc060d1e387'),
    a2b('040c23b1abb7f7e3d2da6ffd70ce9e6f5bf90467c0e1f2e708483d2e61220f0a'
        '0257110d695bec78ac1e15333219d7ba3f8f2f155b76acd56d99680031d83853'
        '99d61262f22e8c70414777cbc060d1e387a4e9ac4624b79e326c19396b44842b'
        'd995123343efe844821ff97ed08e38db59141ed8185359f76121d5fce7c4491d'
        '902551bdd9bbd28e0ae27d1d4c9a6c1a9bb7b8aa36d1b1f6cce0425739'),
    a2b('67e0df0b8e5131766340c895553c13053332fdee1fbd2d9cdde22a331a49aaa1'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'COMPAT-TEST1'
  ),
  (
    a2b('b3a89ed5a7fb6685a67db54c62e663e7'),
    b'Test message!!',
    a2b('b3a89ed5a7fb6685a67db54c62e663e7'),
    a2b('04b9d46d1f333baf6896ce7b64d344092671795438b1dc35a21b0d13b004f28a1c'
        'edd4f1f7ff63106772270050cb62152b07e9c02bbee79db7a3fb4155c464e0d5b3'
        'a89ed5a7fb6685a67db54c62e663e70fed2b44ce0f705e9a84a09978b82f6c603e'
        'b6e6923d592f22193fb7ba0e1765ecd4861ec46c138d85b7206dbd41'),
    a2b('6fdaf5e2e11dd61c116222c748d99b45f69031c9d4d3d5787a9a0fdd3b9c471a'),
    _DEFAULT_SALT,
    _DEFAULT_INFO,
    'COMPAT-TEST2'
  )
)


#
# Command format.
#
# WIDTH	        FIELD
# 1             OP
# 1             MSB IN LEN
# 1             LSB IN LEN
# IN_LEN        IN
# 1             MSB AUTH_DATA LEN
# 1             LSB AUTH_DATA LEN
# 16            IV
# 1             MSB PUB_X LEN
# 1             LSB PUB_X LEN
# PUB_X_LEN     PUB_X
# 1             MSB PUB_Y LEN
# 1             LSB PUB_Y LEN
# PUB_Y_LEN     PUB_Y
# 1             MSB SALT LEN
# 1             LSB SALT LEN
# SALT_LEN      SALT
# 1             MSB INFO LEN
# 1             LSB INFO LEN
# INFO_LEN      INFO
#
_ECIES_CMD_FORMAT = '{o:c}{inl:s}{input}{al:s}{iv}{xl:s}{x}{yl:s}{y}{sl:s}{s}{il:s}{i}'

def _encrypt_cmd(auth, input, iv, pubx, puby, salt, info):
  return _ECIES_OPCODES['ENCRYPT'].to_bytes(1, "big")  +\
         len(auth+input).to_bytes(2, "big") + auth + input +\
         len(auth).to_bytes(2, "big") + iv +\
         len(pubx).to_bytes(2, "big") + pubx +\
         len(puby).to_bytes(2, "big") + puby +\
         len(salt).to_bytes(2, "big") + salt +\
         len(info).to_bytes(2, "big") + info


def _decrypt_cmd(auth, input, iv, d, salt, info):
  return _ECIES_OPCODES['DECRYPT'].to_bytes(1, "big")  +\
         len(input).to_bytes(2, "big") + input +\
         len(auth).to_bytes(2, "big") + iv +\
         len(d).to_bytes(2, "big") + d +\
         bytes([0, 0]) +\
         len(salt).to_bytes(2, "big") + salt +\
         len(info).to_bytes(2, "big") + info


def _ecies_test(tpm):
  for data in _ECIES_INPUTS:
    auth, input, iv, d, pubx, puby, salt, info = data[:-1]
    test_name = 'ECIES-TEST:%s' % data[-1]
    cmd = _encrypt_cmd(auth, input, iv, pubx, puby, salt, info)
    wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.ECIES, cmd))
    encrypted = tpm.unwrap_ext_response(subcmd.ECIES, wrapped_response)
    # check length of encrypted.
    if not encrypted:
      raise subcmd.TpmTestError('%s error:%s' % (
          test_name, 'null encrypted'))

    cmd = _decrypt_cmd(auth, encrypted, iv, d, salt, info)
    wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.ECIES, cmd))
    decrypted = tpm.unwrap_ext_response(subcmd.ECIES, wrapped_response)

    expected = auth + input
    if decrypted != expected:
      raise subcmd.TpmTestError('%s error:%s:%s' % (
          test_name, utils.hex_dump(decrypted), utils.hex_dump(expected)))
    print('%sSUCCESS: %s' % (utils.cursor_back(), test_name))


def _compat_test(tpm):
  for data in _ECIES_COMPAT_INPUTS:
    auth, plaintext, iv, ciphertext, d, salt, info = data[:-1]
    test_name = 'ECIES-TEST:%s' % data[-1]

    cmd = _decrypt_cmd(auth, ciphertext, iv, d, salt, info)
    wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.ECIES, cmd))
    decrypted = tpm.unwrap_ext_response(subcmd.ECIES, wrapped_response)

    expected = auth + plaintext
    if decrypted != expected:
      raise subcmd.TpmTestError('%s error:%s:%s' % (
          test_name, utils.hex_dump(decrypted), utils.hex_dump(expected)))
    print('%sSUCCESS: %s' % (utils.cursor_back(), test_name))


def ecies_test(tpm):
  _ecies_test(tpm)
  _compat_test(tpm)

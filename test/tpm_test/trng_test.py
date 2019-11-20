#!/usr/bin/env python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for trng."""
from __future__ import print_function
import struct

import subcmd
import utils

TRNG_TEST_FMT = '>HB'
TRNG_TEST_RSP_FMT = '>H2IH'
TRNG_TEST_CC = 0x33
TRNG_SAMPLE_SIZE = 1000 # minimal recommended by NIST is 1000 bytes per sample
TRNG_SAMPLE_COUNT = 1000 # NIST require at least 1000000 of 8-bit samples

def get_random_command(size, op):
  return struct.pack(TRNG_TEST_FMT, size, op)

def get_random_command_rsp(size):
  return struct.pack(TRNG_TEST_RSP_FMT, 0x8001,
                     struct.calcsize(TRNG_TEST_RSP_FMT) + size, 0, TRNG_TEST_CC)


def trng_test(tpm, trng_mode):
  """Download entropy samples from TRNG

    Command structure, shared out of band with the test running on the target:

    field     |    size  |                  note
    ========================================================================
    text_len  |    2     | size of the text to process, big endian
    type      |    1     | 0 = TRNG, 1 = CR50 DRBG, 2 = get_entropy
  Args:
    tpm: a tpm object used to communicate with the device
    trng_mode: type of RNG which data will be collected

  Raises:
    subcmd.TpmTestError: on unexpected target responses
  """
  if trng_mode not in [0,1,2]:
    print('%sUnknown random source: %d' % (utils.cursor_back(), trng_mode))
    return
  with open('/tmp/trng_output', 'ab') as f:
    for x in range(0, TRNG_SAMPLE_COUNT):
      wrapped_response = tpm.command(tpm.wrap_ext_command(TRNG_TEST_CC,
                                     get_random_command(TRNG_SAMPLE_SIZE,
                                     trng_mode)))
      if wrapped_response[:12] != get_random_command_rsp(TRNG_SAMPLE_SIZE):
        raise subcmd.TpmTestError("Unexpected response to '%s': %s" %
                                 ("trng", utils.hex_dump(wrapped_response)))
      f.write(wrapped_response[12:])
      print('%s %d%%\r' %( utils.cursor_back(), (x/10)), end=""),
  print('%sSUCCESS: trng' % (utils.cursor_back()))

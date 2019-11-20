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

# should be in sync with TRNG configuration
TRNG_SAMPLE_BITS = 1
# NIST require at least 1000000 of 1-bit samples
TRNG_SAMPLE_COUNT = 1000000

def get_random_command(size, op):
  return struct.pack(TRNG_TEST_FMT, size, op)

def get_random_command_rsp(size):
  return struct.pack(TRNG_TEST_RSP_FMT, 0x8001,
                     struct.calcsize(TRNG_TEST_RSP_FMT) + size, 0, TRNG_TEST_CC)

# convert input packed byte array to n-bits in a byte representation
# used by NIST tests. It's designed to recover sequence of samples
# as it comes from TRNG, including the fact that rand_bytes() reverse
# byte order in every 32-bit chunk.
def to_bitstring(s, n = 1):
  out = ''
  val_left = 0
  bits_left = 0
  while len(s):
    (val) = struct.unpack('>I', s[0:4].rjust(4,'\0'))
    val = (val[0] << bits_left) + val_left
    bits_left += 8 * len(s[0:4])
    s = s[4:]
    while bits_left >= n:
      out = out + struct.pack('B', val & ((1 << n) - 1))
      val >>= n
      bits_left -= n
    val_left = val
  return out

def trng_test(tpm, trng_mode):
  """Download entropy samples from TRNG

    Command structure, shared out of band with the test running on the target:

    field     |    size  |                  note
    ========================================================================
    text_len  |    2     | size of the text to process, big endian
    type      |    1     | 0 = TRNG, 1 = CR50 DRBG, 2 = get fips trng
  Args:
    tpm: a tpm object used to communicate with the device
    trng_mode: type of RNG which data will be collected

  Raises:
    subcmd.TpmTestError: on unexpected target responses
  """
  if trng_mode not in [0,1,2]:
    print('%sUnknown random source: %d' % (utils.cursor_back(), trng_mode))
    return
  # minimal recommended by NIST is 1000 samples per block
  # this variable should be divisible by 4 to match 32bit reads from TRNG
  # make sure number of bytes divides to 4 * TRNG_SAMPLE_BITS
  sample_size = int(1024 / (32 * TRNG_SAMPLE_BITS)) * 32 * TRNG_SAMPLE_BITS
  remaining_samples = TRNG_SAMPLE_COUNT
  with open('/tmp/trng_output', 'ab') as f:
    while remaining_samples:
      wrapped_response = tpm.command(tpm.wrap_ext_command(TRNG_TEST_CC,
                                     get_random_command(sample_size),
				     trng_mode))
      if wrapped_response[:12] != get_random_command_rsp(sample_size):
        raise subcmd.TpmTestError("Unexpected response to '%s': %s" %
                                 ("trng", utils.hex_dump(wrapped_response)))
      bits = to_bitstring(wrapped_response[12:], TRNG_SAMPLE_BITS)
      bits = bits[:remaining_samples]
      f.write(bits)
      remaining_samples -= len(bits)
      print('%s %d%%\r' %( utils.cursor_back(),
                        (((TRNG_SAMPLE_COUNT - remaining_samples)*100)
                         / TRNG_SAMPLE_COUNT)), end="")
  print('%sSUCCESS: %s' % (utils.cursor_back(), 'trng'))

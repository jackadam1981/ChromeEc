#!/usr/bin/env python3
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for trng."""
from __future__ import print_function
#for Python3 use line below
#from math import gcd
from fractions import gcd
import struct
import subcmd
import utils

TRNG_TEST_FMT = '>H'
TRNG_TEST_RSP_FMT = '>H2IH'
TRNG_TEST_CC = 0x33

# should be in sync with TRNG configuration
TRNG_SAMPLE_BITS = 1
# NIST require at least 1000000 of 1-bit samples
TRNG_SAMPLE_COUNT = 1000000

def get_random_command(size):
  return struct.pack(TRNG_TEST_FMT, size)

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
    val = (struct.unpack('>I', s[0:4].rjust(4,'\0'))[0] << bits_left) + val_left
    bits_left += 8 * len(s[0:4])
    s = s[4:]
    while bits_left >= n:
      out = out + struct.pack('B', val & ((1 << n) - 1))
      val >>= n
      bits_left -= n
    val_left = val
  return out

def trng_test(tpm, trng_output):
  """Download entropy samples from TRNG

    Command structure, shared out of band with the test running on the target:

    field     |    size  |                  note
    ===================================================================
    text_len  |    2     | size of the text to process, big endian

  Args:
    tpm: a tpm object used to communicate with the device

  Raises:
    subcmd.TpmTestError: on unexpected target responses
  """
  # minimal recommended by NIST is 1000 samples per block
  # TRNG_SAMPLE_BITS is internal setting for TRNG which is important for
  # entropy analysis. TRNG internally gets a 16 bit sample (measurement of
  # time to collapse ring oscillator). Then some slice of these bits
  # [0:TRNG_SAMPLE_BITS] is added (packed) to TRNG FIFO which has internal
  # size of 2048 bits. Reading from TRNG always return 32 bits from FIFO.
  # To extract actual samples from packed 32-bit reading, need to reverse
  # and split read 32-bit into individual samples, each size TRNG_SAMPLE_BITS
  # bits. As it's only possible to read 32 bits at once and TRNG_SAMPLE_BITS
  # can be anything from 1 to 16, including non-power of 2, need to ensure
  # readings to result in non-fractional number of samples. At the same time
  # NIST requires minimum 1000 samples at once, and we also want to reduce
  # number of read requests which are adding overhead.
  # this variable should be divisible by 4 to match 32bit reads from TRNG
  # make sure number of bytes divides to 4 * TRNG_SAMPLE_BITS
  # compute least common multiplier of 1000 * TRNG_SAMPLE_BITS and 32
  # and convert it in bytes
  sample_size = (1000 * TRNG_SAMPLE_BITS * 32 /
                 gcd(1000 * TRNG_SAMPLE_BITS, 32)) / 8
  # combine reads of small batches if possible
  sample_size = max(1, int((1500 / sample_size))) * sample_size
  remaining_samples = TRNG_SAMPLE_COUNT
  with open(trng_output, 'wb') as f:
    while remaining_samples:
      wrapped_response = tpm.command(tpm.wrap_ext_command(TRNG_TEST_CC,
                                     get_random_command(sample_size)))
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
  print('%sSUCCESS: %s' % (utils.cursor_back(), trng_output))

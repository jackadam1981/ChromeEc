#!/usr/bin/env python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Insert proper RW_HASH (SHA1) into ec.RW.flat.


  Example:
    util/add_rw_hash.ph <board> <infile> <outfile>

    util/add_rw_hash.py zinger ec.RW.flat ec.RW.flat
"""
import hashlib
import logging
import os
import sys

SHA1_SIZE = 32


def get_flash_size(board):
  """Get flash size for particular EC device.

  Parses following line from ec.RW.map:
    'FLASH            0x0000000008010000 0x0000000000010000 xr'
  and returns size

  Args:
    board: string of board name used to locate mapfile

  Returns:
    integer size of flash device to be written
  """
  ec_map = os.path.join(os.getcwd(), 'build', board, 'ec.RW.map')
  size_str = '0'
  with open(ec_map) as fd:
    for ln in fd.readline():
      if ln.startswith('FLASH'):
        (_, _, size_str, _) = ln.split()
        break
  return int(size_str, 16)


def main():
  if len(sys.argv) != 4:
    logging.fatal('%s <board> <input> <output>', sys.argv[0])

  flash_size = get_flash_size(sys.argv[1])
  # rw can only occupy at most half of EC flash
  max_rw_fw_size = flash_size - SHA1_SIZE
  with open(sys.argv[2]) as fd:
    fw = fd.read()

  fw_size = len(fw)
  # Compute SHA-1 hash for the full (padded) RW firmware
  padded_fw = fw + '\xff' * (max_rw_fw_size - fw_size)
  sha = hashlib.sha1(padded_fw).digest()

  with open(sys.argv[3], 'w') as fd:
    fd.write(padded_fw)
    fd.write(sha)

if __name__ == '__main__':
  try:
    main()
  except KeyboardInterrupt:
    sys.exit()


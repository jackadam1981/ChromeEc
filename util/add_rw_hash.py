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
import sys

from flash_utils import get_rw_flash_size

SHA1_ALIGN = 32


def main():
  if len(sys.argv) != 4:
    print 'USAGE:\n\t%s <board> <input> <output>' % sys.argv[0]
    sys.exit(-1)

  flash_size = get_rw_flash_size(sys.argv[1])
  # rw can only occupy at most half of EC flash
  max_rw_fw_size = flash_size - SHA1_ALIGN
  with open(sys.argv[2]) as fd:
    fw = fd.read()

  fw_size = len(fw)
  if fw_size > max_rw_fw_size:
    logging.error('%d fw_size can\'t be greater than max_rw_fw_size %d',
                  fw_size, max_rw_fw_size)
    sys.exit(-1)

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


# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for manipulating EC flash."""
import os


def get_rw_flash_size(board):
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
    for ln in fd.readlines():
      if ln.startswith('FLASH'):
        (_, _, size_str, _) = ln.split()
        break
  return int(size_str, 16)

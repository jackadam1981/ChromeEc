#!/usr/bin/env python
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
import argparse
import ctypes
import os


class Header(ctypes.Structure):
  _pack_ = 1
  _fields_ = [
      ('signature', ctypes.c_uint32),
      ('ftb_ver', ctypes.c_uint32),
      ('chip_id', ctypes.c_uint32),
      ('svn_ver', ctypes.c_uint32),
      ('fw_ver', ctypes.c_uint32),
      ('config_id', ctypes.c_uint32),
      ('config_ver', ctypes.c_uint32),
      ('reserved2', ctypes.c_uint8 * 8),
      ('release_info', ctypes.c_ulonglong),
      ('sec0_size', ctypes.c_uint32),
      ('sec1_size', ctypes.c_uint32),
      ('sec2_size', ctypes.c_uint32),
      ('sec3_size', ctypes.c_uint32),
      ('crc', ctypes.c_uint32),
  ]

FW_HEADER_SIZE = 64
FW_HEADER_SIGNATURE = 0xAA55AA55
FW_FTB_VER = 0x00000001
FW_BYTES_ALIGN = 4
FW_BIN_VER_OFFSET = 16
FW_BIN_CONFIG_ID_OFFSET = 20

FLASH_ADDR_CODE = 0x0000 * 4
FLASH_ADDR_CX = 0x7000 * 4
FLASH_ADDR_CONFIG = 0x7C00 * 4

OUTPUT_FILE_SIZE = 128 * 1024  # 128KB


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', '-i', required=True)
  parser.add_argument('--output', '-o', required=True)
  args = parser.parse_args()

  with open(args.input) as f:
    bs = f.read()

  size = len(bs)
  if size < FW_HEADER_SIZE + FW_BYTES_ALIGN:
    raise Exception('FW size too small')

  print('FTB file size:', size)

  header = Header()
  assert ctypes.sizeof(header) == FW_HEADER_SIZE

  ctypes.memmove(ctypes.addressof(header), bs, ctypes.sizeof(header))
  if (header.signature != FW_HEADER_SIGNATURE or
      header.ftb_ver != FW_FTB_VER or
      header.chip_id != 0x3936):
    raise Exception('Invalid header')

  for key, _ in header._fields_:
    if key.startswith('reserved'):
      continue
    v = getattr(header, key)
    if isinstance(v, ctypes.Array):
      print(key, map(hex, v))
    else:
      print(key, hex(v))

  dimension = (header.sec0_size +
               header.sec1_size +
               header.sec2_size +
               header.sec3_size)

  assert dimension + FW_HEADER_SIZE + FW_BYTES_ALIGN == size
  data = bs[FW_HEADER_SIZE:FW_HEADER_SIZE + dimension]

  with open(args.output, 'wb') as f:
    # ensure the file size
    f.seek(OUTPUT_FILE_SIZE - 1, os.SEEK_SET)
    f.write('\x00')

    offset = 0

    # write each sections
    f.seek(FLASH_ADDR_CODE, os.SEEK_SET)
    f.write(data[offset : offset + header.sec0_size])
    offset += header.sec0_size

    f.seek(FLASH_ADDR_CONFIG, os.SEEK_SET)
    f.write(data[offset : offset + header.sec1_size])
    offset += header.sec1_size

    f.seek(FLASH_ADDR_CX, os.SEEK_SET)
    f.write(data[offset : offset + header.sec2_size])
    offset += header.sec2_size

    f.flush()


if __name__ == '__main__':
  main()

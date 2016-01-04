 #!/usr/bin/python
 # Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import hashlib
import struct

import subcmd

def upgrade(tpm):
  """Exercise the upgrade command.

  cmd         1  value of 2
  block_base  4  address of the block to write
  digest      4  first 4 bytes of sha1 of the block
  data      var
  """

  cmd  = struct.pack('>I', 0)  # address
  cmd += struct.pack('>I', 0)  # data (a nooop)
  wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.FW_UPGRADE, cmd))
  base = struct.unpack('>I', tpm.unwrap_ext_response(subcmd.FW_UPGRADE,
                                                     wrapped_response))[0]
  if base == 0x44000:
    fname = 'build/cr50/RW/ec.RW_B.flat'
    base = 0x84000
  else:
    fname = 'build/cr50/RW/ec.RW.flat'
    base = 0x44000

  data = open(fname, 'r').read()
  transferred = 0
  block_size = 1024

  while transferred < len(data):
    h = hashlib.sha1()
    tx_size = min(block_size, len(data) - transferred)
    chunk = data[transferred:transferred+tx_size]
    cmd  = struct.pack('>I', base)  # address
    h.update(chunk)
    cmd += h.digest()[0:4]
    cmd += chunk
    resp = tpm.unwrap_ext_response(subcmd.FW_UPGRADE,
                                   tpm.command(tpm.wrap_ext_command(
                                     subcmd.FW_UPGRADE, cmd)))
    code = ord(resp[0][0])
    if code:
      print('%x - resp %d' % (base, ))
      break
    base += tx_size
    transferred += tx_size

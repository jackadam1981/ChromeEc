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

  cmd         1  value of FW_UPGRADE
  digest      4  first 4 bytes of sha1 of the remainder of the message
  block_base  4  address of the block to write
  data      var
  """

  cmd  = struct.pack('>I', 0)  # address
  cmd += struct.pack('>I', 0)  # data (a noop)
  wrapped_response = tpm.command(tpm.wrap_ext_command(subcmd.FW_UPGRADE, cmd))
  base_str = tpm.unwrap_ext_response(subcmd.FW_UPGRADE, wrapped_response)
  if len(base_str) < 4:
    raise subcmd.TpmTestError('Initialization error %d' %
                              ord(base_str[0]))
  base = struct.unpack('>I', base_str)[0]
  print('base %x\n' % base)
  if base == 0x84000:
    fname = 'build/cr50/RW/ec.RW_B.flat'
  elif base == 0x44000:
    fname = 'build/cr50/RW/ec.RW.flat'
  else:
    raise subcmd.TpmTestError('Unknown base address 0x%x' % base)
  data = open(fname, 'r').read()
  transferred = 0
  block_size = 1024

  while transferred < len(data):
    tx_size = min(block_size, len(data) - transferred)
    chunk = data[transferred:transferred+tx_size]
    cmd  = struct.pack('>I', base)  # address
    h = hashlib.sha1()
    h.update(cmd)
    h.update(chunk)
    cmd = h.digest()[0:4] + cmd + chunk
    resp = tpm.unwrap_ext_response(subcmd.FW_UPGRADE,
                                   tpm.command(tpm.wrap_ext_command(
                                     subcmd.FW_UPGRADE, cmd)))
    code = ord(resp[0][0])
    if code:
      raise subcmd.TpmTestError('%x - resp %d' % (base, code))
    base += tx_size
    transferred += tx_size

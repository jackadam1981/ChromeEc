# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import usb.core
import time

VID = 0x18d1
PID = 0x502b

device = usb.core.find(idVendor=VID, idProduct=PID)
configuration = device.get_active_configuration()
interface = configuration[(4, 1)]
interface.set_altsetting()
endpoint = interface[0]

while True:
  xs = list(endpoint.read(128))
  if xs:
    print ''.join(['%02x' % x for x in xs])
  time.sleep(0.001)

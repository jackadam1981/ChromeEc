#!/usr/bin/env python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pexpect
import sys

child = pexpect.spawn('build/emu/{0}/{0}.exe'.format(sys.argv[1]), timeout=5)
result_id = child.expect([pexpect.TIMEOUT, 'Pass!', 'Fail!'])
if result_id == 0:
  print 'Test timed out after 5 seconds!'
  sys.exit(1)
elif result_id == 1:
  print 'Test passed!'
  sys.exit(0)
elif result_id == 2:
  print 'Test failed!'
  sys.exit(1)

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Simple test as an example
#

import time

def test(helper):
      helper.wait_output("Inits done")
      helper.ec_command("")
      helper.ec_command("settemp 0 330")
      helper.wait_output("set fan: 2200", timeout=11)
      helper.ec_command("settemp 0 340")
      helper.wait_output("set fan: 4400", timeout=11)
      helper.ec_command("settemp 0 337")
      for i in xrange(12):
          helper.wait_output("set fan: 4400", timeout=2)
      helper.ec_command("settemp 0 400")
      helper.wait_output("set fan: 4400")
      helper.ec_command("settemp 0 338")
      helper.ec_command("settemp 1 330")
      for i in xrange(12):
          helper.wait_output("set fan: 4400", timeout=2)
      helper.ec_command("settemp 1 334")
      helper.wait_output("set fan: 8800", timeout=11)
      return True
      helper.ec_command("version")
      ro = helper.wait_output("RO:\s*(?P<ro>\S+)", use_re=True)["ro"]
      wa = helper.wait_output("A:\s*(?P<a>\S+)", use_re=True)["a"]
      wb = helper.wait_output("B:\s*(?P<b>\S*)", use_re=True)["b"]
      helper.trace("Version (RO/A/B) %s / %s / %s\n" % (ro, wa, wb))
      return True # PASS !

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Power button debounce test
#

import time

def check_no_output(helper, reg_ex):
    success = False
    try:
        helper.wait_output(reg_ex, use_re=True, timeout=1)
    except:
        success = True
    return success

def consume_output(helper, reg_ex):
    done = False
    while not done:
        try:
            helper.wait_output(reg_ex, use_re=True, timeout=1)
        except:
            done = True

def test(helper):
      helper.wait_output("--- UART initialized")
      # Release power button
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      consume_output(helper, "PB released")

      helper.trace("Press power button for ~10us and check this event is\n" +
                   "ignored\n")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(0.01)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      if not check_no_output(helper, "PB released"):
          return False

      helper.trace("Press power button for ~50us and check this event is\n" +
                   "treated as a single press.")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(0.05)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      helper.wait_output("PB released", timeout=1)
      if not check_no_output(helper, "PB released"):
          return False

      helper.trace("Press power button for two consecutive ~20us period\n" +
                   "and check this event is ignored\n")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(0.02)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      time.sleep(0.02)
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(0.02)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      if not check_no_output(helper, "PB released"):
          return False

      helper.trace("Press power button for ~20us, release for ~20us, and\n" +
                   "then press for ~50us. Check this is treated as a single\n" +
                   "press\n")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(0.02)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      time.sleep(0.02)
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(0.05)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      helper.wait_output("pwrbtn=LOW", timeout=1)
      helper.wait_output("pwrbtn=HIGH", timeout=1)
      if not check_no_output(helper, "pwrbtn=LOW"):
          return False

      helper.trace("Hold down power button, wait for power button press\n" +
                   "sent out. Then relase for ~20us, check power button\n" +
                   "release is not sent out.\n")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      helper.wait_output("pwrbtn=LOW", timeout=1)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      time.sleep(0.01)
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      if not check_no_output(helper, "PB released"):
          return False
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      helper.wait_output("PB released", timeout=1)

      helper.trace("Hold down power button for ~40us and check a single\n" +
                   "press is sent out\n")
      consume_output(helper, "pwrbtn=")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(0.04)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      helper.wait_output("pwrbtn=LOW", timeout=1)
      helper.wait_output("pwrbtn=HIGH", timeout=1)
      if not check_no_output(helper, "pwrbtn=LOW"):
          return False

      helper.trace("Hold down power button for 1s and check a single press\n" +
                   "is sent out within 100us\n")
      consume_output(helper, "pwrbtn=")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(1)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      #if not check_no_output(helper, "Send power button press keycode"):
      #    return False
      t_low = helper.wait_output("\[(?P<t>[\d\.]+) PB PCH pwrbtn=LOW\]",
                                 use_re=True)["t"]
      t_high = helper.wait_output("\[(?P<t>[\d\.]+) PB PCH pwrbtn=HIGH\]",
                                  use_re=True)["t"]
      if not check_no_output(helper, "pwrbtn=LOW"):
          return False
      if float(t_high) - float(t_low) >= 0.1:
          return False

      helper.trace("Hold down power button for 5s and check two presses\n" +
                   "are sent out\n")
      consume_output(helper, "pwrbtn=")
      helper.ec_command("gpiomock POWER_BUTTONn 0")
      time.sleep(5)
      helper.ec_command("gpiomock POWER_BUTTONn 1")
      helper.wait_output("pwrbtn=LOW", timeout=1)
      helper.wait_output("pwrbtn=HIGH", timeout=1)
      helper.wait_output("pwrbtn=LOW", timeout=1)
      helper.wait_output("pwrbtn=HIGH", timeout=1)
      if not check_no_output(helper, "pwrbtn=LOW"):
          return False

      return True # PASS !

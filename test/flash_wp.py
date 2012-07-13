# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Flash write-protect test
#

def test(helper):
    helper.wait_output("--- UART initialized")

    # Deassert write protect pin. Apply lock. Nothing should happen.
    helper.ec_command("gpiomock write_protect 0")
    helper.ec_command("flashwp set 0x0 0x1000")
    helper.ec_command("flashwp lock")
    if not helper.check_no_output("Protect block"):
        return False
    helper.ec_command("flashwp unlock")

    # Assert write protect pin. Apply lock. Flash should be protected.
    helper.ec_command("gpiomock write_protect 1")
    helper.ec_command("flashwp lock")
    # Block size = 0x800. Lock 0x0-0x1000 should protect block 0-1.
    helper.wait_output("Protect block 0")
    helper.wait_output("Protect block 1")

    return True

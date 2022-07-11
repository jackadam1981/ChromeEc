# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Construct the drivers test binaries"""


from httplib2 import KeyCerts


drivers = register_host_test(
    test_name="drivers",
    dts_overlays=[
        here / "overlay.dts",
    ],
    kconfig_files=[
        here / "prj.conf",
    ],
)

isl923x = drivers.variant(
    project_name="test-drivers-isl923x",
)

led_driver = drivers.variant(
    project_name="test-drivers-led_driver",
    dts_overlays=[
        here / "led_driver" / "led_pins.dts",
        here / "led_driver" / "led_policy.dts",
    ],
    kconfig_files=[here / "led_driver" / "prj.conf"],
)

# Run all the tests (that aren't split to subdirs) with CONFIG_EC_HOST_CMD enabled
ec_host_cmd = drivers.variant(
    project_name="test-drivers-ec_host_cmd",
    kconfig_files=[here / "ec_host_cmd.conf"],
)


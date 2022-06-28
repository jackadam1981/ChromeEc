# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Construct the drivers test binaries"""


def register_drivers_variant(
    project_name, extra_dts_overlays=(), extra_kconfig_files=()
):
    """Register a variant of the drivers test"""
    return register_host_test(
        project_name=project_name,
        dts_overlays=[
            here / "overlay.dts",
            # Project-specific DTS customization.
            *extra_dts_overlays,
        ],
        kconfig_files=[
            # Common to all projects.
            here / "prj.conf",
            # Project-specific KConfig customization.
            *extra_kconfig_files,
        ],
    )


drivers = register_drivers_variant(
    project_name="drivers",
)

isl923x = register_drivers_variant(
    project_name="drivers-isl923x",
)

led_driver = register_drivers_variant(
    project_name="drivers-led_driver",
    extra_dts_overlays=[
        here / "led_driver" / "led_pins.dts",
        here / "led_driver" / "led_policy.dts",
    ],
    extra_kconfig_files=[here / "led_driver" / "prj.conf"],
)

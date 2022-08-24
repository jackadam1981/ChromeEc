# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for skyrim."""


def register_skyrim_project(
    project_name,
    chip="npcx9m3f",
    extra_dts_overlays=(),
    extra_kconfig_files=(),
):
    """Register a variant of corsola."""
    register_npcx_project(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[
            # Common to all projects.
            here / "adc.dts",
            here / "fan.dts",
            here / "gpio.dts",
            here / "i2c.dts",
            here / "interrupts.dts",
            here / "keyboard.dts",
            here / "motionsense.dts",
            here / "usbc.dts",
            # Project-specific DTS customizations.
            *extra_dts_overlays,
        ],
        kconfig_files=[here / "prj.conf", *extra_kconfig_files],
    )


register_skyrim_project(
    project_name="skyrim",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "skyrim.dts",
        here / "battery_skyrim.dts",
        here / "led_pins_skyrim.dts",
        here / "led_policy_skyrim.dts",
    ],
    extra_kconfig_files=[
        here / "prj_skyrim.conf",
    ],
)


register_skyrim_project(
    project_name="winterhold",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "winterhold/winterhold.dts",
        here / "winterhold/battery_winterhold.dts",
        here / "winterhold/led_pins_winterhold.dts",
        here / "winterhold/led_policy_winterhold.dts",
    ],
    extra_kconfig_files=[
        here / "winterhold/prj_winterhold.conf",
    ],
)

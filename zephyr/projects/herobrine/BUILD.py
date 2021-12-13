# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def register_variant(project_name, extra_dts_overlays=(), extra_kconfig_files=()):
    register_npcx_project(
        project_name=project_name,
        zephyr_board="npcx9",
        dts_overlays=[
            # Common to all projects.
            here / "adc.dts",
            here / "battery.dts",
            here / "common.dts",
            here / "i2c.dts",
            here / "interrupts.dts",
            here / "pwm.dts",
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


register_variant(
    project_name="herobrine",
    extra_kconfig_files=[here / "prj_herobrine.conf"],
    extra_dts_overlays=[
        here / "gpio.dts",
        here / "motionsense.dts",
        here / "switchcap.dts",
        here / "usbc.dts",
    ],
)


register_variant(
    project_name="hoglin",
    extra_kconfig_files=[here / "prj_hoglin.conf"],
    extra_dts_overlays=[
        here / "gpio_hoglin.dts",
        here / "motionsense_hoglin.dts",
        here / "switchcap_hoglin.dts",
        here / "usbc_hoglin.dts",
    ],
)

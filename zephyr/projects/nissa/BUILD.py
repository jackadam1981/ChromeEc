# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for nissa."""

# Nivviks and Craask, Pujjo, Xivu has NPCX993F, Nereid and Joxer has ITE81302


def register_nissa_project(
    project_name,
    chip="it81302bx",
    extra_dts_overlays=(),
    extra_kconfig_files=(),
):
    """Register a variant of nissa."""
    register_func = register_binman_project
    if chip.startswith("npcx"):
        register_func = register_npcx_project

    return register_func(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=["cbi.dts", *extra_dts_overlays],
        kconfig_files=[here / "prj.conf", *extra_kconfig_files],
    )


nivviks = register_nissa_project(
    project_name="nivviks",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "nivviks/generated.dts",
        here / "nivviks/cbi.dts",
        here / "nivviks/overlay.dts",
        here / "nivviks/motionsense.dts",
        here / "nivviks/keyboard.dts",
        here / "nivviks/power_signals.dts",
        here / "nivviks/pwm_leds.dts",
    ],
    extra_kconfig_files=[here / "nivviks/prj.conf"],
)

nereid = register_nissa_project(
    project_name="nereid",
    chip="it81302bx",
    extra_dts_overlays=[
        here / "nereid/generated.dts",
        here / "nereid/overlay.dts",
        here / "nereid/motionsense.dts",
        here / "nereid/keyboard.dts",
        here / "nereid/power_signals.dts",
        here / "nereid/pwm_leds.dts",
    ],
    extra_kconfig_files=[here / "nereid/prj.conf"],
)

craask = register_nissa_project(
    project_name="craask",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "craask/generated.dts",
        here / "craask/overlay.dts",
        here / "craask/motionsense.dts",
        here / "craask/keyboard.dts",
        here / "craask/power_signals.dts",
        here / "craask/pwm_leds.dts",
    ],
    extra_kconfig_files=[here / "craask/prj.conf"],
)

pujjo = register_nissa_project(
    project_name="pujjo",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "pujjo/generated.dts",
        here / "pujjo/overlay.dts",
        here / "pujjo/motionsense.dts",
        here / "pujjo/keyboard.dts",
        here / "pujjo/power_signals.dts",
        here / "pujjo/pwm_leds.dts",
    ],
    extra_kconfig_files=[here / "pujjo/prj.conf"],
)

xivu = register_nissa_project(
    project_name="xivu",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "xivu/generated.dts",
        here / "xivu/overlay.dts",
        here / "xivu/motionsense.dts",
        here / "xivu/keyboard.dts",
        here / "xivu/power_signals.dts",
        here / "xivu/led_pins.dts",
        here / "xivu/led_policy.dts",
    ],
    extra_kconfig_files=[here / "xivu/prj.conf"],
)

joxer = register_nissa_project(
    project_name="joxer",
    chip="it81302bx",
    extra_dts_overlays=[
        here / "joxer/generated.dts",
        here / "joxer/overlay.dts",
        here / "joxer/motionsense.dts",
        here / "joxer/keyboard.dts",
        here / "joxer/power_signals.dts",
        here / "joxer/pwm_leds.dts",
    ],
    extra_kconfig_files=[here / "joxer/prj.conf"],
)

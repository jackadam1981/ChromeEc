# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for corsola."""

# Default chip is it81202bx, some variants will use NPCX9X.


def register_corsola_project(
    project_name,
    chip="it81202bx",
    extra_dts_overlays=(),
    extra_kconfig_files=(),
):
    """Register a variant of corsola."""
    register_func = register_binman_project
    if chip.startswith("npcx"):
        register_func = register_npcx_project

    register_func(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[
            here / "common.dts",
            here / "power_signal.dts",
            here / "usba.dts",
            *extra_dts_overlays,
        ],
        kconfig_files=[here / "program.conf", *extra_kconfig_files],
    )


register_corsola_project(
    "krabby",
    extra_dts_overlays=[
        here / "ite_adc.dts",
        here / "battery_krabby.dts",
        here / "ite_gpio.dts",
        here / "ite_keyboard.dts",
        here / "i2c_krabby.dts",
        here / "ite_interrupts.dts",
        here / "led_krabby.dts",
        here / "ite_motionsense.dts",
        here / "ite_usbc.dts",
    ],
    extra_kconfig_files=[
        here / "ite_program.conf",
        here / "prj_krabby.conf",
    ],
)

register_corsola_project(
    project_name="kingler",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "npcx_adc.dts",
        here / "battery_kingler.dts",
        here / "npcx_host_interface.dts",
        here / "npcx_i2c.dts",
        here / "npcx_interrupts.dts",
        here / "npcx_gpio.dts",
        here / "npcx_keyboard.dts",
        here / "led_kingler.dts",
        here / "npcx_motionsense.dts",
        here / "npcx_usbc.dts",
        here / "npcx_default_gpio_pinctrl.dts",
    ],
    extra_kconfig_files=[
        here / "npcx_program.conf",
        here / "prj_kingler.conf",
    ],
)

register_corsola_project(
    project_name="steelix",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "npcx_adc.dts",
        here / "battery_steelix.dts",
        here / "npcx_host_interface.dts",
        here / "npcx_i2c.dts",
        here / "npcx_interrupts.dts",
        here / "interrupts_steelix.dts",
        here / "cbi_steelix.dts",
        here / "gpio_steelix.dts",
        here / "npcx_keyboard.dts",
        here / "keyboard_steelix.dts",
        here / "led_steelix.dts",
        here / "npcx_motionsense.dts",
        here / "motionsense_steelix.dts",
        here / "usba_steelix.dts",
        here / "npcx_usbc.dts",
        here / "npcx_default_gpio_pinctrl.dts",
    ],
    extra_kconfig_files=[
        here / "npcx_program.conf",
        here / "prj_steelix.conf",
    ],
)


register_corsola_project(
    "tentacruel",
    extra_dts_overlays=[
        here / "adc_tentacruel.dts",
        here / "battery_tentacruel.dts",
        here / "cbi_tentacruel.dts",
        here / "gpio_tentacruel.dts",
        here / "ite_keyboard.dts",
        here / "i2c_tentacruel.dts",
        here / "interrupts_tentacruel.dts",
        here / "led_tentacruel.dts",
        here / "motionsense_tentacruel.dts",
        here / "usbc_tentacruel.dts",
        here / "thermistor_tentacruel.dts",
    ],
    extra_kconfig_files=[
        here / "ite_program.conf",
        here / "prj_tentacruel.conf",
    ],
)

register_corsola_project(
    "magikarp",
    extra_dts_overlays=[
        here / "adc_magikarp.dts",
        here / "battery_magikarp.dts",
        here / "cbi_magikarp.dts",
        here / "gpio_magikarp.dts",
        here / "ite_keyboard.dts",
        here / "i2c_magikarp.dts",
        here / "interrupts_magikarp.dts",
        here / "led_magikarp.dts",
        here / "motionsense_magikarp.dts",
        here / "usbc_magikarp.dts",
    ],
    extra_kconfig_files=[
        here / "ite_program.conf",
        here / "prj_magikarp.conf",
    ],
)

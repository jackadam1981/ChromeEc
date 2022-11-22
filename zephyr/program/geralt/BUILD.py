# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for geralt."""

register_raw_project(
    project_name="geralt",
    zephyr_board="it82202ax",
    dts_overlays=[
        "adc.dtsi",
        "gpio.dtsi",
        "i2c.dtsi",
        "interrupts.dtsi",
        "pwm.dtsi",
        "motionsense.dtsi",
    ],
)

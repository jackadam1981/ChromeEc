# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for starfish."""

register_raw_project(
    project_name="starfish_v1",
    zephyr_board="stm32g473",
    dts_overlays=[
        "clock.dts",
        "flash.dts",
        "gpio.dts",
        "usb.dts",
        "uart.dts",
    ],
)

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CCG6 example project."""

register_npcx_project(
    project_name="ccg6",
    zephyr_board="npcx9m3f",
    dts_overlays=[
        here / "gpio.dts",
        here / "i2c.dts",
        here / "interrupts.dts",
    ],
)

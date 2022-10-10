# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for bloonchipper."""

register_binman_project(
    project_name="bloonchipper",
    zephyr_board="stm32f412cg",
    supported_toolchains=["coreboot-sdk"],
    dts_overlays=[
        "gpio.dts",
        "binman.dts",
    ],
)

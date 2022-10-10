# Copyright 2023 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for bloonchipper."""

register_binman_project(
    project_name="bloonchipper",
    zephyr_board="google_dragonclaw",
    modules=["ec", "hal_stm32", "cmsis"],
    supported_toolchains=["coreboot-sdk"],
    dts_overlays=[
        "bloonchipper.dts",
    ],
)

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

register_binman_project(
    project_name="mec172x",
    zephyr_board="mec172x_evb",
    dts_overlays=[
        "gpio.dts",
        "interrupts.dts",
        ],
)

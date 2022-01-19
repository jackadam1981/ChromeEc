# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

register_npcx_project(
    project_name="adlrvpp_npcx",
    zephyr_board="adlrvpp_npcx",
    dts_overlays=[
        "gpio.dts",
        "pwm.dts",
        ],
)

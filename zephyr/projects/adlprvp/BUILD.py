# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

register_npcx_project(
    project_name="adlprvp",
    zephyr_board="npcx9",
    dts_overlays=["gpio.dts", "i2c.dts","fiu.dts"],
)

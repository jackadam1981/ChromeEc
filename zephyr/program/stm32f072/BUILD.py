# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def register_stm32_kb(project_name, board_name):
    """Register a board."""

    register_stm32_project(
        project_name=project_name,
        zephyr_board=board_name,
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=[
            here / project_name / "project.conf",
        ],
    )


register_stm32_kb("kb", "stm32f072xb")
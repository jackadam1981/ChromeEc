# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for hdctools."""


def register_stm32(project_name, board_name):
    """Register a board."""

    register_raw_project(
        project_name=project_name,
        zephyr_board=board_name,
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=[
            here / project_name / "project.conf",
        ],
        modules = ["cmsis", "ec", "hal_stm32"],
    )


register_stm32("starfish_v1", "stm32g473")

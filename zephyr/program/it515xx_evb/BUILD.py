# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for it515xx_evb."""


def register_it515xx_evb_project(project_name, zephyr_board):
    """Register a variant of the it515xx_evb"""
    register_raw_project(
        project_name=project_name,
        zephyr_board=zephyr_board,
        # Project-specific devicetree overlay
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        # Project-specific KConfig customization.
        kconfig_files=[
            here / project_name / "project.conf",
        ],
    )


register_it515xx_evb_project(
    project_name="it515xx_evb", zephyr_board="it51xxx/it51526bw"
)

#assert_rw_fwid_DO_NOT_EDIT(project_name="it515xx_evb", addr=0xbffe0)

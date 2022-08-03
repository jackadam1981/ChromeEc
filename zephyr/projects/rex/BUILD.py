# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rex Projects."""


def register_variant(
    project_name, chip="npcx9m3f", extra_dts_overlays=(), extra_kconfig_files=()
):
    """Register a variant of rex."""
    return register_npcx_project(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[
            # Common to all projects.
            here / "rex.dts",
            # Project-specific DTS customization.
            *extra_dts_overlays,
        ],
        kconfig_files=[
            # Common to all projects.
            here / "prj.conf",
            # Project-specific KConfig customization.
            *extra_kconfig_files,
        ],
    )


rex0 = register_variant(
    project_name="rex0",
    extra_dts_overlays=[
        here / "generated.dts",
        here / "interrupts.dts",
    ],
    extra_kconfig_files=[here / "prj_rex0.conf"],
)

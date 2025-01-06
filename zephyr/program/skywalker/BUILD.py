# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for skywalker."""


def register_skywalker_project(
    project_name,
    chip="npcx9/npcx9m7fb",
    kconfig_files=None,
    **kwargs,
):
    """Register a variant of skywalker."""
    if kconfig_files is None:
        kconfig_files = [
            # Common to all projects.
            here / "program.conf",
            here / "npcx_program.conf",
            # Project-specific KConfig customization.
            here / project_name / "project.conf",
        ]

    register_npcx_project(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=kconfig_files,
        inherited_from=["skywalker"],
        **kwargs,
    )

register_skywalker_project(
    project_name="skywalker"
)

# Note for reviews, do not let anyone edit these assertions, the addresses
# must not change after the first RO release.
assert_rw_fwid_DO_NOT_EDIT(project_name="skywalker", addr=0x7ffe0)

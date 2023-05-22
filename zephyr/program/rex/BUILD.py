# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for Rex."""


def register_rex_project(
    project_name,
    kconfig_files=None,
    dts_overlays=None,
):
    """Register a variant of Rex."""
    if kconfig_files is None:
        kconfig_files = [
            # Common to all projects.
            here / "program.conf",
            # Project-specific KConfig customization.
            here / project_name / "project.conf",
        ]

    if dts_overlays is None:
        dts_overlays=[
            here / project_name / "project.overlay",
        ]
    register_npcx_project(
        project_name=project_name,
        zephyr_board="npcx9m7f",
        dts_overlays=dts_overlays,
        kconfig_files=kconfig_files,
        inherited_from=["rex"],
    )

register_rex_project(
    project_name="rex",
    kconfig_files=[
        # Common to all projects.
        here / "program.conf",
        # Parent project's config
        here / "rex" / "project.conf",
    ] + [ here / "rex-sans-sensors" / "project.conf" ] if \
	"CONFIG_ISH_ENABLE=y" in open(here / "program.conf").read() \
	else None, dts_overlays=[ here / "rex-sans-sensors" / \
	"project.overlay" ] if "CONFIG_ISH_ENABLE=y" in \
	open(here / "program.conf").read() else None,
)

register_rex_project(
    project_name="screebo",
)

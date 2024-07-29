# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for trulo."""


def register_trulo_project(
    project_name,
):
    """Register a variant of trulo."""
    register_binman_project(
        project_name=project_name,
        zephyr_board="npcx9m3f",
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=[
            # Common to all projects.
            here / "program.conf",
            # Project-specific KConfig customization.
            here / project_name / "project.conf",
        ],
        inherited_from=["trulo"],
    )


register_trulo_project(
    project_name="trulo",
)

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for brox-sku4."""


def register_brox_sku4_project(
    project_name,
    kconfig_files=None,
):
    """Register a variant of brox-sku4."""
    if kconfig_files is None:
        kconfig_files = [
            # Common to all projects.
            here / "program.conf",
            # Project-specific KConfig customization.
            here / project_name / "project.conf",
        ]

    return register_binman_project(
        project_name=project_name,
        zephyr_board="it8xxx2/it82002aw",
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=kconfig_files,
        inherited_from=["brox-sku4"],
    )


brox_sku4 = register_brox_sku4_project(
    project_name="brox-sku4",
    kconfig_files=[
        # Common to all projects.
        here / "program.conf",
        # Parent project's config
        here / "brox-sku4" / "project.conf",
        # Common sensor configs
        here / "motionsense.conf",
    ],
)

# Note for reviews, do not let anyone edit these assertions, the addresses
# must not change after the first RO release. Not needed for brox-ish since it
# doesn't use RO+RW
assert_rw_fwid_DO_NOT_EDIT(project_name="brox-sku4", addr=0x60098)

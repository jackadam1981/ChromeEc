# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for dirks."""


def register_dirks_project(
    project_name,
    chip="it8xxx2/it81302bx",
    kconfig_files=None,
):
    """Register a variant of Dirks."""
    register_func = register_binman_project
    if kconfig_files is None:
        kconfig_files = [
            here / "program.conf",
            here / "it8xxx2_program.conf",
            here / project_name / "project.conf",
        ]

    return register_func(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[here / project_name / "project.overlay"],
        kconfig_files=kconfig_files,
        inherited_from=["dirks"],
    )


dirks = register_dirks_project(
    project_name="dirks",
    chip="it8xxx2/it81302bx",
)

# Note for reviews, do not let anyone edit these assertions, the addresses
# must not change after the first RO release.
assert_rw_fwid_DO_NOT_EDIT(project_name="dirks", addr=0xBFFE0)

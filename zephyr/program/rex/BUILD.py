# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for Rex."""


def register_variant(
    project_name,
    kconfig_files=None,
    dts_files=None,
):
    """Register a variant of Rex."""
    if kconfig_files is None:
        kconfig_files = []
    if dts_files is None:
        dts_files = []

    # Add DTS files for the project (if found)
    if (here / project_name / "project.overlay").is_file():
        dts_files.insert(0, here / project_name / "project.overlay")

    # Project-specific KConfig customization (if found)
    if (here / project_name / "project.conf").is_file():
        kconfig_files.insert(0, here / project_name / "project.conf")

    # Common to all projects.
    kconfig_files.insert(0, here / "program.conf")
    register_npcx_project(
        project_name=project_name,
        zephyr_board="npcx9m7f",
        dts_overlays=dts_files,
        kconfig_files=kconfig_files,
    )


register_variant(
    project_name="rex",
    dts_files=[here / "sensor_interrupts.dts", here / "motionsense.dts"],
)

register_variant(
    project_name="rex-sans-sensors",
    kconfig_files=[here / "rex" / "project.conf", here / "sans-sensors.conf"],
    dts_files=[here / "rex" / "project.overlay"],
)

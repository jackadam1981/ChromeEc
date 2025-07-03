# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for trulo."""


def register_trulo_project(
    project_name,
    chip="npcx9/npcx9m3f",
    zephyr_board=None,
    kconfig_files=None,
):
    """Register a variant of Trulo."""
    if zephyr_board is None:
        zephyr_board = chip

    if "it8" in zephyr_board:
        register_binman_project(
            project_name=project_name,
            zephyr_board=zephyr_board,
            dts_overlays=[
                here / project_name / "project.overlay",
            ],
            kconfig_files=kconfig_files + [here / "dsp_comms.conf"],
            modules=["cmsis", "cmsis_6", "picolibc", "ec", "pigweed", "nanopb"],
            inherited_from=["trulo"],
            **kwargs,
        )
    else:
        register_npcx_project(
            project_name=project_name,
            zephyr_board=zephyr_board,
            dts_overlays=[
                here / project_name / "project.overlay",
            ],
            kconfig_files=kconfig_files + [here / "dsp_comms.conf"],
            inherited_from=["trulo"],
            modules=["cmsis", "cmsis_6", "picolibc", "ec", "pigweed", "nanopb"],
            **kwargs,
        )


def register_trulo_binman_project(
    project_name,
    kconfig_files=None,
):
    """Register a variant of trulo."""
    if kconfig_files is None:
        kconfig_files = [
            # Common to all projects.
            here / "program.conf",
            # Project-specific KConfig customization.
            here / project_name / "project.conf",
        ]
    return register_trulo_project(
        project_name=project_name,
<<<<<<< HEAD   (624abc628e7f410f3fde0e1906aba29ac67380c4 pujjocento: modify Touchpanel sequence)
        zephyr_board=chip,
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=kconfig_files,
        inherited_from=["trulo"],
||||||| BASE
        zephyr_board=chip,
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=kconfig_files + [here / "dsp_comms.conf"],
        inherited_from=["trulo"],
        modules=["cmsis", "cmsis_6", "picolibc", "ec", "pigweed", "nanopb"],
        **kwargs,
=======
        zephyr_board="it8xxx2/it82002aw",
        kconfig_files=kconfig_files,
>>>>>>> CHANGE (a3a42d77390ad35a4c4e00e953feb22422702648 kaladin: Add initial program files for trulo with ite chip)
    )


register_trulo_project(
    project_name="kaladin",
    zephyr_board="it8xxx2/it82002bw",
    kconfig_files=[
        # ite's config
        here / "ite.conf",
        # Common to all projects.
        here / "ite_program.conf",
        # Parent project's config
        here / "kaladin" / "project.conf",
    ],
)

register_trulo_project(
    project_name="trulo",
)

register_trulo_project(
    project_name="pujjocento",
    chip="npcx9/npcx9m7fb",
    kconfig_files=[
        # Common to all projects.
        here / "program.conf",
        # Parent project's config
        here / "pujjocento" / "project.conf",
    ],
)

register_trulo_project(
    project_name="pujjolo",
    kconfig_files=[
        # Common to all projects.
        here / "program.conf",
        # Parent project's config
        here / "pujjolo" / "project.conf",
    ],
)

register_trulo_project(
    project_name="uldrenite",
    chip="npcx9/npcx9m7fb",
    kconfig_files=[
        # Common to all projects.
        here / "program.conf",
        # Parent project's config
        here / "uldrenite" / "project.conf",
    ],
)

# Note for reviews, do not let anyone edit these assertions, the addresses
# must not change after the first RO release.
assert_rw_fwid_DO_NOT_EDIT(project_name="trulo", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="pujjocento", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="pujjolo", addr=0x40144)
<<<<<<< HEAD   (624abc628e7f410f3fde0e1906aba29ac67380c4 pujjocento: modify Touchpanel sequence)
assert_rw_fwid_DO_NOT_EDIT(project_name="uldrenite", addr=0x40144)
||||||| BASE
assert_rw_fwid_DO_NOT_EDIT(project_name="trulo-ti", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="uldrenite", addr=0x40144)
=======
assert_rw_fwid_DO_NOT_EDIT(project_name="trulo-ti", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="uldrenite", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="kaladin", addr=0xBFFE0)
>>>>>>> CHANGE (a3a42d77390ad35a4c4e00e953feb22422702648 kaladin: Add initial program files for trulo with ite chip)

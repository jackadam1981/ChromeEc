# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for corsola."""

# Default chip is it81202bx, some variants will use NPCX9X.


def register_corsola_project(
    project_name,
    chip="it81202bx",
    extra_dts_overlays=(),
    extra_kconfig_files=(),
):
    """Register a variant of corsola."""
    register_func = register_binman_project
    if chip.startswith("npcx"):
        register_func = register_npcx_project

    register_func(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[
            here / "common.dtsi",
            here / "power_signal.dtsi",
            here / "usba.dtsi",
            *extra_dts_overlays,
        ],
        kconfig_files=[here / "program.conf", *extra_kconfig_files],
    )


register_corsola_project(
    "krabby",
    extra_dts_overlays=[
        here / "krabby/project.overlay",
    ],
    extra_kconfig_files=[
        here / "ite_program.conf",
        here / "krabby/project.conf",
    ],
)

register_corsola_project(
    project_name="kingler",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "kingler/project.overlay",
    ],
    extra_kconfig_files=[
        here / "npcx_program.conf",
        here / "kingler/project.conf",
    ],
)

register_corsola_project(
    project_name="steelix",
    chip="npcx9m3f",
    extra_dts_overlays=[
        here / "steelix/project.overlay",
    ],
    extra_kconfig_files=[
        here / "npcx_program.conf",
        here / "steelix/project.conf",
    ],
)


register_corsola_project(
    "tentacruel",
    extra_dts_overlays=[
        here / "tentacruel/project.overlay",
    ],
    extra_kconfig_files=[
        here / "ite_program.conf",
        here / "tentacruel/project.conf",
    ],
)

register_corsola_project(
    "magikarp",
    extra_dts_overlays=[
        here / "magikarp/project.overlay",
    ],
    extra_kconfig_files=[
        here / "ite_program.conf",
        here / "magikarp/project.conf",
    ],
)

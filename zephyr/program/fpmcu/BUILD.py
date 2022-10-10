# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for FPMCUs."""


def register_fpmcu_variant(
    project_name,
    zephyr_board,
    variant_modules=(),
    variant_dts_overlays=(),
    variant_kconfig_files=(),
):
    """Register FPMCU project"""
    return register_binman_project(
        project_name=project_name,
        zephyr_board=zephyr_board,
        modules=["ec", *variant_modules],
        supported_toolchains=["coreboot-sdk"],
        dts_overlays=[*variant_dts_overlays],
        kconfig_files=[here / "prj.conf", *variant_kconfig_files],
    )


bloonchipper = register_fpmcu_variant(
    project_name="bloonchipper",
    zephyr_board="google_dragonclaw",
    variant_modules=["hal_stm32", "cmsis"],
    variant_dts_overlays=[
        here / "bloonchipper" / "bloonchipper.dts",
        here / "bloonchipper" / "rdp1_quirk.dts",
    ],
    variant_kconfig_files=[here / "bloonchipper" / "prj.conf"],
)

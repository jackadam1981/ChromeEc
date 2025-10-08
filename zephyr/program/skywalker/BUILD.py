# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for skywalker."""


def register_skywalker_npcx_project(project_name):
    """Register a variant of skywalker."""
    return register_npcx_project(
        project_name=project_name,
        zephyr_board="npcx9/npcx9m7fb",
        dts_overlays=[here / project_name / "project.overlay"],
        kconfig_files=[
            here / "program.conf",
            here / project_name / "project.conf",
        ],
        inherited_from=["skywalker"],
    )


def register_skywalker_ite_project(project_name):
    """Register a variant of skywalker using ITE EC."""
    return register_binman_project(
        project_name=project_name,
        zephyr_board="it8xxx2/it82002aw",
        dts_overlays=[here / project_name / "project.overlay"],
        kconfig_files=[
            here / "program.conf",
            here / "ite_program.conf",
            here / project_name / "project.conf",
        ],
        inherited_from=["skywalker"],
    )


register_skywalker_npcx_project(project_name="skywalker")
register_skywalker_npcx_project(project_name="skywalker-tps6699x")
register_skywalker_ite_project(project_name="luuke")
register_skywalker_npcx_project(project_name="obiwan")
register_skywalker_npcx_project(project_name="yoda")
register_skywalker_ite_project(project_name="anakin")
register_skywalker_ite_project(project_name="baze")
register_skywalker_ite_project(project_name="tarkin")
<<<<<<< HEAD   (1a9478ed621654e0af34b8857313d074ac0aa5cf pujjolo: Change EN_USB_A0_VBUS to correct pin)
||||||| BASE   (d29cbb1c7df42374a2dfcadcf432005a08cff92d bluey: Add initial devicetree configs for keyboard)
register_skywalker_ite_project(project_name="padme")
=======
register_skywalker_ite_project(project_name="padme")
register_skywalker_ite_project(project_name="grogu")
>>>>>>> CHANGE (01ff989710682061c1a6ace9071283ddea094465 Grogu: Initial zephyr config for grogu project)

# Note for reviews, do not let anyone edit these assertions, the addresses
# must not change after the first RO release.
assert_rw_fwid_DO_NOT_EDIT(project_name="skywalker", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="skywalker-tps6699x", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="luuke", addr=0x60098)
assert_rw_fwid_DO_NOT_EDIT(project_name="obiwan", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="yoda", addr=0x40144)
assert_rw_fwid_DO_NOT_EDIT(project_name="anakin", addr=0x60098)
assert_rw_fwid_DO_NOT_EDIT(project_name="baze", addr=0x60098)
assert_rw_fwid_DO_NOT_EDIT(project_name="tarkin", addr=0x60098)
<<<<<<< HEAD   (1a9478ed621654e0af34b8857313d074ac0aa5cf pujjolo: Change EN_USB_A0_VBUS to correct pin)
||||||| BASE   (d29cbb1c7df42374a2dfcadcf432005a08cff92d bluey: Add initial devicetree configs for keyboard)
assert_rw_fwid_DO_NOT_EDIT(project_name="padme", addr=0x60098)
=======
assert_rw_fwid_DO_NOT_EDIT(project_name="padme", addr=0x60098)
assert_rw_fwid_DO_NOT_EDIT(project_name="grogu", addr=0x60098)
>>>>>>> CHANGE (01ff989710682061c1a6ace9071283ddea094465 Grogu: Initial zephyr config for grogu project)

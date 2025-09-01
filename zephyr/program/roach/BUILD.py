# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for roach."""


def register_variant(project_name, rwsig_sign=True):
    """Register a variant of Roach."""

    signer_kwarg = {}
    if rwsig_sign:
        # pylint: disable=undefined-variable
        signer_kwarg["signer"] = signers.RwsigSigner(here / "dev_key.pem")

    register_binman_project(
        project_name=project_name,
        zephyr_board="it8xxx2/it82202ax/512",
        dts_overlays=[here / project_name / "project.overlay"],
        kconfig_files=[
            here / "program.conf",
            here / project_name / "project.conf",
        ],
        **signer_kwarg,
    )


<<<<<<< HEAD   (dbded63f0910c95df4dc75a9394030e9814dd521 padme: Enable three buttons entry recovery mode)
register_variant("roach")
register_variant("axii", rwsig_sign=False)
register_variant("kelpie")
register_variant("spikyrock")
||||||| BASE   (06b2c9cc594fb33246b49ebb33024159a7679aca ectool: Refactor fingerprint frame download using libec)
# Keyboard tester
register_variant("axii", rwsig_sign=False, inherited_from=[])
# Detachable keyboards
register_variant("roach", inherited_from=["geralt"])
register_variant("kelpie", inherited_from=["geralt"])
register_variant("spikyrock", inherited_from=["staryu"])
=======
# Keyboard tester
register_variant("axii", rwsig_sign=False, inherited_from=[])
# Detachable keyboards
register_variant("roach", inherited_from=["geralt"])
register_variant("kelpie", inherited_from=["geralt"])
register_variant("spikyrock", inherited_from=["staryu"])
register_variant("eirtae", inherited_from=["jedi"])
>>>>>>> CHANGE (c9c086a7b6f3b68bd97791c39e93dfaa93e00ad2 Roach: Eirtae: initial board)

# Note for reviews, do not let anyone edit these assertions, the addresses
# must not change after the first RO release.
assert_rw_fwid_DO_NOT_EDIT(project_name="axii", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="kelpie", addr=0x40098)
assert_rw_fwid_DO_NOT_EDIT(project_name="roach", addr=0x40098)
assert_rw_fwid_DO_NOT_EDIT(project_name="spikyrock", addr=0x40098)
assert_rw_fwid_DO_NOT_EDIT(project_name="eirtae", addr=0x40098)

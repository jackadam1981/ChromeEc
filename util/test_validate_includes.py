# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for validate_includes.py.

Usage: pytest test_validate_includes.py
"""

import pathlib
import site

import pytest

site.addsitedir(pathlib.Path(__file__).parent)
import validate_includes


PLATFORM_EC = pathlib.Path(__file__).parent.parent.resolve()


def test_get_ec_include_groups():
    graph = validate_includes.get_ec_include_groups()

    # Spot-check a few groups.
    common = graph["common"]
    assert common.parents == []
    assert PLATFORM_EC / "include" in common.paths

    cros_ec = graph["cros_ec"]
    assert cros_ec.parents == [common]
    assert PLATFORM_EC / "builtin" in cros_ec.paths

    core_cortex_m = graph["cros_ec_core_cortex-m"]
    assert core_cortex_m.parents == [cros_ec]
    assert PLATFORM_EC / "core" / "cortex-m" in core_cortex_m.paths

    chip_npcx = graph["cros_ec_chip_npcx"]
    assert chip_npcx.parents == [core_cortex_m]
    assert PLATFORM_EC / "chip" / "npcx" in chip_npcx.paths

    baseboard_volteer = graph["cros_ec_baseboard_volteer"]
    assert baseboard_volteer.parents == [cros_ec]
    assert PLATFORM_EC / "baseboard" / "volteer" in baseboard_volteer.paths

    board_delbin = graph["cros_ec_board_delbin"]
    assert board_delbin.parents == [chip_npcx]
    assert PLATFORM_EC / "board" / "delbin" in board_delbin.paths
    assert PLATFORM_EC / "baseboard" / "volteer" in board_delbin.paths


def test_validate_ok(tmp_path):
    path1 = tmp_path / "path1"
    path1.mkdir()

    (path1 / "a.h").touch()
    (path1 / "b.h").touch()
    (path1 / "bad_but_in_burndown.h").touch()

    path2 = tmp_path / "path2"
    path2.mkdir()

    (path2 / "c.h").touch()
    (path2 / "d.h").touch()
    (path2 / "bad_but_in_burndown.h").touch()

    graph = {}
    validate_includes.add_include_group(
        graph,
        "common",
        paths=[path1],
    )
    validate_includes.add_include_group(
        graph,
        "subgroup",
        parents=["common"],
        paths=[path2],
    )

    validate_includes.validate_includes(
        graph,
        burndown_list={"bad_but_in_burndown.h"},
    )


def test_validate_fail(tmp_path):
    path1 = tmp_path / "path1"
    path1.mkdir()

    (path1 / "a.h").touch()
    (path1 / "b.h").touch()
    (path1 / "bad.h").touch()

    path2 = tmp_path / "path2"
    path2.mkdir()

    (path2 / "c.h").touch()
    (path2 / "d.h").touch()
    (path2 / "bad.h").touch()

    graph = {}
    validate_includes.add_include_group(
        graph,
        "common",
        paths=[path1],
    )
    validate_includes.add_include_group(
        graph,
        "subgroup",
        parents=["common"],
        paths=[path2],
    )

    with pytest.raises(validate_includes.IncludeGraphValidationError):
        validate_includes.validate_includes(graph)

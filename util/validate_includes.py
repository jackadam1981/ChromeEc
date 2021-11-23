#!/usr/bin/env python3
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Validate that all include directories have unique filenames.

It's very easy to accidentally make conflicting file names in multiple
EC include paths (e.g., in "include/" and "zephyr/shim/include/") and
be misled that this provides some sort of "override" logic.

Unfortunately, the C compiler has no include ordering, and this will
create undefined behavior between different toolchains (e.g., LLVM vs
GCC, and even different versions of these toolchains).

This check runs both as a repo pre-upload hook and in the CQ to keep
the include paths deterministic.

This script is by no means intended to be perfect: only the EC build
system and cmake actually know about the real include paths.  It's
simply intended to be the best approximation that we can get without
spinning up the build systems.
"""

import dataclasses
import pathlib
import re
import sys
import textwrap


# Headers in the tree that are known to have collisions.  Please help
# clean up this list as you fix issues.  Do not add new headers to
# this list.
BURNDOWN_LIST = {
    "bluetooth_le.h",  # Both in /include and /chip/nrf51
    "builtin/assert.h",  # Both in / and /zephyr/src/shim/include
    "usb_pd_pdo.h",  # Both in /include and /baseboard/kalista
    "usb_pd_policy.h",  # Both in /include and /baseboard/kukui
    "vpd_api.h",  # Both in /test and /board/chocodile_vpdmcu
}


# The concept of an include group is that, for a given group group, we
# inherit zero or more other include groups.  For example, the zephyr
# include group inherits from common.  These include groups create the
# rules for which paths can share include file names, and which paths
# cannot.  If two headers with the same name exist within the same
# include group or any of its parents' groups, this will cause an
# error unless the header is explicitly listed in the burndown list.
@dataclasses.dataclass()
class IncludeGroup:
    name: str
    parents: "list[IncludeGroup]" = dataclasses.field(default_factory=list)
    paths: "list[pathlib.Path]" = dataclasses.field(default_factory=list)


def add_include_group(graph, name, parents=(), paths=()):
    graph[name] = IncludeGroup(
        name,
        parents=[graph[parent] for parent in parents],
        paths=paths,
    )


def get_ec_include_groups():
    """Get the well-known EC include groups graph."""
    ec_dir = pathlib.Path(__file__).parent.parent.resolve()
    graph = {}
    add_include_group(
        graph,
        "common",
        paths=[
            ec_dir,
            ec_dir / "fuzz",
            ec_dir / "include",
            ec_dir / "include" / "driver",
            ec_dir / "test",
            ec_dir / "third_party",
        ],
    )
    add_include_group(
        graph,
        "cros_ec",
        parents=["common"],
        paths=[
            ec_dir / "builtin",
        ],
    )
    add_include_group(
        graph,
        "zephyr",
        parents=["common"],
        paths=[
            ec_dir / "zephyr" / "shim" / "include",
        ],
    )

    for core_dir in (ec_dir / "core").iterdir():
        if (core_dir / "build.mk").exists():
            add_include_group(
                graph,
                f"cros_ec_core_{core_dir.name}",
                parents=["cros_ec"],
                paths=[core_dir],
            )

    # Good enough detection of core name from chip build.mk
    coredef_p = re.compile(r"CORE\s*:?=\s*([a-z0-9_-]+)")
    for chip_dir in (ec_dir / "chip").iterdir():
        build_mk = chip_dir / "build.mk"
        if build_mk.exists():
            contents = build_mk.read_text()
            match = coredef_p.search(contents)
            if match:
                parent = f"cros_ec_core_{match.group(1)}"
            else:
                parent = "cros_ec"
            add_include_group(
                graph,
                f"cros_ec_chip_{chip_dir.name}",
                parents=[parent],
                paths=[chip_dir],
            )

    # Similar logic for baseboard
    chipdef_p = re.compile(r"CHIP\s*:?=\s*([a-z0-9_-]+)")
    for baseboard_dir in (ec_dir / "baseboard").iterdir():
        build_mk = baseboard_dir / "build.mk"
        if build_mk.exists():
            contents = build_mk.read_text()
            match = chipdef_p.search(contents)
            if match:
                parent = f"cros_ec_chip_{match.group(1)}"
            else:
                parent = "cros_ec"
            add_include_group(
                graph,
                f"cros_ec_baseboard_{baseboard_dir.name}",
                parents=[parent],
                paths=[baseboard_dir],
            )

    # Finally, for board.  Logic is slightly different this time as
    # not all boards have a baseboard.
    bbdef_p = re.compile(r"BASEBOARD\s*:?=\s*([a-z0-9_-]+)")
    for board_dir in (ec_dir / "board").iterdir():
        build_mk = board_dir / "build.mk"
        if build_mk.exists():
            contents = build_mk.read_text()

            match_chip = chipdef_p.search(contents)
            match_bb = bbdef_p.search(contents)

            if match_chip:
                parent = f"cros_ec_chip_{match_chip.group(1)}"
            elif match_bb:
                parent = f"cros_ec_baseboard_{match_bb.group(1)}"
            else:
                parent = "cros_ec"

            # Since if we override chip and we still have a baseboard,
            # we need to special case adding the baseboard include dir
            # here without adding the baseboard include group itself
            # (since it may inherit from a different chip).
            paths = [board_dir]
            if match_bb:
                paths.append(ec_dir / "baseboard" / match_bb.group(1))

            add_include_group(
                graph,
                f"cros_ec_board_{board_dir.name}",
                parents=[parent],
                paths=paths,
            )

    return graph


class IncludeGraphValidationError(Exception):
    pass


def validate_includes(graph, burndown_list=()):
    """Actually do the include validation.

    Args:
        graph: The include graph (likely generated by
            get_ec_include_groups).
        burndown_list: An optional list of include names which we will
            ignore collisions.

    Raises:
        IncludeGraphValidationError: if the include graph contains
            duplicate reachable include file paths.
    """

    def _get_all_include_paths(group):
        all_includes = set()
        visited = set()

        def _rec(group):
            if group.name in visited:
                return
            visited.add(group.name)
            all_includes.update(group.paths)
            for parent in group.parents:
                _rec(parent)

        _rec(group)
        return all_includes

    # A cache of reachable headers to reduce I/O on subsequent calls
    # to duplicated include paths.
    reachable_headers = {}

    def _get_reachable_headers(path):
        if path not in reachable_headers:
            reachable_headers[path] = [
                str(header.relative_to(path)) for header in path.rglob("*.h")
            ]
        return reachable_headers[path]

    for group in graph.values():
        paths = _get_all_include_paths(group)
        seen_headers = {}
        for path in paths:
            for header in _get_reachable_headers(path):
                if header in seen_headers:
                    if header in burndown_list:
                        continue
                    raise IncludeGraphValidationError(
                        f"While validating include group {group.name}, I found that "
                        f"the header {header} is visible to the C compiler under both "
                        f"the include path {seen_headers[header]} and {path}.  This "
                        f"will cause undeterministic behavior based on the toolchain "
                        f"used, as the C standard does not say anything about "
                        f"the ordering of include paths.  Please use unique file "
                        f"names for both headers."
                    )
                seen_headers[header] = path


def main():
    graph = get_ec_include_groups()
    try:
        validate_includes(graph, burndown_list=BURNDOWN_LIST)
    except IncludeGraphValidationError as e:
        print(
            *textwrap.wrap(str(e), width=80),
            sep="\n",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()

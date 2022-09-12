#!/usr/bin/env vpython3

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script modifies sources to enable more debugging symbols by disabling clang
optimization. Why? Because if we just set the options to -O0, we lose LTO. When
we lose LTO, various symbols that never get used and never defined remain within
the executable causing an undefined symbol error.

This script is intended to be used in addition to the enabling debug compiler
option.
"""

# [VPYTHON:BEGIN]
# python_version: "3.8"
# [VPYTHON:END]

import argparse
import glob
import itertools
import sys

PRAGMA_DEBUG_BEGIN = "/* PRAGMA DEBUG BEGIN */"
PRAGMA_DEBUG_END = "/* PRAGMA DEBUG END */"
PRAGMA_DEBUG_MACRO = "#pragma clang optimize off"

PRAGMA_DEBUG = "\n".join(
    [
        PRAGMA_DEBUG_BEGIN,
        PRAGMA_DEBUG_MACRO,
        PRAGMA_DEBUG_END,
        "\n",
    ]
)


def main(argv):
    """Add/remove #pragma clang optimize off to C sources."""

    description = """Adds/removes clang debug pragma from sources to
    enable/disable debug symbols. Is idempotent."""

    parser = argparse.ArgumentParser(description=description)

    parser.add_argument("--yes", help="Enable debugging", action="store_true")
    parser.add_argument("--no", help="Disable debugging", action="store_true")

    args = parser.parse_args(argv)

    if args.yes and args.no:
        print("--yes and --no are mutually exclusive. Pick exactly one.")
        sys.exit(1)
    elif not args.yes and not args.no:
        print("No argument given. Pick --yes xor --no")
        sys.exit(1)

    add_debug = args.yes

    sources = itertools.chain(
        glob.glob("./builtin/**/*.c", recursive=True),
        glob.glob("./common/**/*.c", recursive=True),
        glob.glob("./core/**/*.c", recursive=True),
        glob.glob("./driver/**/*.c", recursive=True),
        glob.glob("./zephyr/**/*.c", recursive=True),
    )

    for src in sources:
        with open(src) as src_fd:
            contents = src_fd.read()

        pragma_debug_inside = PRAGMA_DEBUG in contents

        if add_debug and not pragma_debug_inside:
            modified_contents = PRAGMA_DEBUG + contents

        elif not add_debug and pragma_debug_inside:
            modified_contents = contents.replace(PRAGMA_DEBUG, "")

        else:
            # Skipping to make script idempotent
            continue

        with open(src, "w") as src_fd:
            src_fd.write(modified_contents)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env vpython3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check Zephyr RAM size.

This script enforces that Zephyr builds have enough free RAM to satisfy
CONFIG_PLATFORM_EC_PRESERVED_END_OF_RAM_SIZE.
"""

# [VPYTHON:BEGIN]
# wheel: <
#   name: "infra/python/wheels/anytree-py2_py3"
#   version: "version:2.8.0"
# >
# wheel: <
#   name: "infra/python/wheels/six-py2_py3"
#   version: "version:1.16.0"
# >
# [VPYTHON:END]

import argparse
import os
from pathlib import Path
import sys
import subprocess


def main(build_path: Path):
    """Run the ROM report."""

    ninja_env = dict(os.environ)
    extra_env_vars = {
        "PYTHON_EXECUTABLE": sys.executable,
    }
    print(f"Python = {sys.executable}")
    ninja_env.update(extra_env_vars)

    cmd = ["ninja", "-C", build_path, "rom_report"]
    try:
        subprocess.run(cmd, env=ninja_env, check=True)
    except subprocess.CalledProcessError:
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board", type=str, help="Zephyr project name")

    args = parser.parse_args()

    build_ro_path = Path("build", "zephyr", args.board, "build-ro")

    print(f"Ninja build path: {build_ro_path}")

    main(build_ro_path)

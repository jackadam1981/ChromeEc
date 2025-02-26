#!/usr/bin/env -S python3 -u
# -*- coding: utf-8 -*-
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide coreboot-sdk-portage-deps to callers

Parse the portage deps from the coreboot-sdk and return a dict containing
the results.
"""

import argparse
import subprocess
from typing import List, Optional


def get_portage_deps(eclass_path):
    """Obtain the list of dependencies from the eclass.

    Args:
        eclass_path: Path to the eclass to parse

    Returns:
        Dict of architectures and the desired version/hash
    """
    run_result = subprocess.run(
        [
            "bash",
            "-c",
            f"EAPI=7; source {eclass_path};"
            ' for key in "${!COREBOOT_SDK_VERSIONS[@]}"; do'
            ' echo "${key}=${COREBOOT_SDK_VERSIONS["${key}"]}";'
            " done",
        ],
        stdout=subprocess.PIPE,
        check=True,
    )
    try:
        return {
            key: tuple(value.split("/"))
            for key, value in [
                line.split("=", 1)
                for line in run_result.stdout.strip().decode().splitlines()
            ]
        }
    except UnicodeDecodeError:
        print(f"Unable to decode the portage deps: {run_result}")
        return {}


def main(argv: Optional[List[str]] = None):
    """Main calling function for the script"""
    parser = argparse.ArgumentParser(description="coreboot_sdk")
    parser.add_argument(
        "--eclass",
        help="Eclass to parse and generate toolchains from",
        default=(
            "/mnt/host/source/src/third_party/chromiumos-overlay/"
            "eclass/coreboot-sdk-ec-dependencies.eclass"
        ),
    )
    parser.add_argument(
        "--output",
        help="Output file path",
        default="/mnt/host/source/src/platform/ec/bazel/portage_deps.bzl",
    )
    opts = parser.parse_args(argv)
    deps = get_portage_deps(opts.eclass)

    with open(opts.output, "w", encoding="utf-8") as f:
        f.write("PORTAGE_DEPS = {\n")
        for arch, (version, sha256) in deps.items():
            f.write(f'    "{arch}": ("{version}", "{sha256}"),\n')
        f.write("}\n")


if __name__ == "__main__":
    main()

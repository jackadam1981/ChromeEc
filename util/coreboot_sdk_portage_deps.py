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

DEFAULT_OUTPUT_PATH="/mnt/host/source/src/platform/ec/bazel/portage_deps.bzl"
DEFAULT_ECLASS=(
            "/mnt/host/source/src/third_party/chromiumos-overlay/"
            "eclass/coreboot-sdk-ec-dependencies.eclass"
        )

def get_portage_deps(eclass_path = DEFAULT_ECLASS, output = DEFAULT_OUTPUT_PATH):
    """Obtain the list of dependencies from the eclass.

    Args:
        eclass_path: Path to the eclass to parse
        output: Path of file to output

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
        return_map = {
            key: tuple(value.split("/"))
            for key, value in [
                line.split("=", 1)
                for line in run_result.stdout.strip().decode().splitlines()
            ]
        }
        with open(output, "w", encoding="utf-8") as f:
            f.write("# Copyright 2025 The ChromiumOS Authors\n")
            f.write("# Use of this source code is governed by a BSD-style license that can be\n")
            f.write("# found in the LICENSE file.\n\n")
            f.write("# This file is auto-generated, do not alter.\n\n")
            f.write("PORTAGE_DEPS = {\n")
            for arch, (version, sha256) in return_map.items():
                f.write(f'    "{arch}": ("{version}", "{sha256}"),\n')
            f.write("}\n")
        return return_map
    except UnicodeDecodeError:
        print(f"Unable to decode the portage deps: {run_result}")
        return {}


def main(argv: Optional[List[str]] = None):
    """Main calling function for the script"""
    parser = argparse.ArgumentParser(description="coreboot_sdk")
    parser.add_argument(
        "--eclass",
        help="Eclass to parse and generate toolchains from",
        default=DEFAULT_ECLASS,
    )
    parser.add_argument(
        "--output",
        help="Output file path",
        default=DEFAULT_OUTPUT_PATH,
    )
    opts = parser.parse_args(argv)
    get_portage_deps(opts.eclass, opts.output)

if __name__ == "__main__":
    main()

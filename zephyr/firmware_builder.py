#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build and test all of the Zephyr boards.

This is the entry point for the custom firmware builder workflow recipe.
"""

import argparse
import collections
import json
import multiprocessing
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys

from google.protobuf import json_format  # pylint: disable=import-error

from chromite.api.gen_sdk.chromite.api import firmware_pb2


def build(opts):
    """Builds all Zephyr firmware targets"""


def bundle(opts):
    """Bundle the artifacts for either firmware or code coverage."""
    pass


def test(opts):
    """Runs all of the unit tests for Zephyr firmware"""
    pass


def check_inherits(_opts):
    """Reads the src/project/*/*/generated/joined.jsonproto files and compares
    the boards and zephyr_ec targets with the zephyr inherited_from values.
    """
    pass


def main(args):
    """Builds and tests all of the Zephyr targets and reports build metrics"""
    opts = parse_args(args)

    # Convert the full version strings (R130-16032.8.0-1) to the short form (16032.8.0).
    if opts.bcs_version:
        match = re.compile(r"R\d+-(\d+\.\d+\.\d+)(-\d+)?").fullmatch(
            opts.bcs_version
        )
        if match:
            opts.bcs_version = match[1]

    if not hasattr(opts, "func"):
        print("Must select a valid sub command!")
        return -1

    # Run selected sub command function
    return opts.func(opts)


def parse_args(args):
    """Parse command line args."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "--cpus",
        default=multiprocessing.cpu_count(),
        help="The number of cores to use.",
    )

    parser.add_argument(
        "--metrics",
        dest="metrics",
        required=False,
        help="File to write the json-encoded MetricsList proto message.",
    )

    parser.add_argument(
        "--metadata",
        required=False,
        help=(
            "Full pathname for the file in which to write build artifact "
            "metadata."
        ),
    )

    parser.add_argument(
        "--output-dir",
        required=False,
        help=(
            "Full pathname for the directory in which to bundle build "
            "artifacts."
        ),
    )

    parser.add_argument(
        "--code-coverage",
        required=False,
        action="store_true",
        help="Build host-based unit tests for code coverage.",
    )

    parser.add_argument(
        "--bcs-version",
        dest="bcs_version",
        default="",
        required=False,
        # TODO(b/180008931): make this required=True.
        help="BCS version to include in metadata.",
    )

    # Would make this required=True, but not available until 3.7
    sub_cmds = parser.add_subparsers()

    build_cmd = sub_cmds.add_parser("build", help="Builds all firmware targets")
    build_cmd.set_defaults(func=build)

    build_cmd = sub_cmds.add_parser(
        "bundle",
        help="Creates a tarball containing build "
        "artifacts from all firmware targets",
    )
    build_cmd.set_defaults(func=bundle)

    test_cmd = sub_cmds.add_parser("test", help="Runs all firmware unit tests")
    test_cmd.set_defaults(func=test)

    check_inherits_cmd = sub_cmds.add_parser(
        "check_inherits",
        help="Checks the inherited_from values against Boxster",
    )
    check_inherits_cmd.set_defaults(func=check_inherits)

    return parser.parse_args(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

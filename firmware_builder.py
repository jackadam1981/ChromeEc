#!/usr/bin/env -S python3 -u
# -*- coding: utf-8 -*-
# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build, bundle, or test all of the EC boards.

This is the entry point for the custom firmware builder workflow recipe.  It
gets invoked by chromite/api/controller/firmware.py.
"""

import argparse
import multiprocessing
import os
import pathlib
import shutil
import subprocess
import sys

# pylint: disable=import-error
from google.protobuf import json_format

from chromite.api.gen_sdk.chromite.api import firmware_pb2


DEFAULT_BUNDLE_METADATA_FILE = "/tmp/artifact_bundle_metadata"


def write_metadata(opts, info):
    """Write the metadata about the bundle."""
    bundle_metadata_file = (
        opts.metadata if opts.metadata else DEFAULT_BUNDLE_METADATA_FILE
    )
    with open(bundle_metadata_file, "w", encoding="utf-8") as file:
        file.write(json_format.MessageToJson(info))


def build(opts):
    """Builds all EC firmware targets"""
    pass


def bundle(opts):
    """Bundle the artifacts."""
    info = firmware_pb2.FirmwareArtifactInfo()  # pylint: disable=no-member
    write_metadata(opts, info)


def test(opts):
    """Runs all of the unit tests for EC firmware"""

    # Install Renode.
    subprocess.run(
        [
            "cipd",
            "ensure",
            "-ensure-file",
            "-",
            "-root",
            "./renode",
        ],
        input="chromiumos/infra/tools/renode latest".encode("utf-8"),
        cwd=os.path.dirname(__file__),
        check=True,
    )

    os.environ["PATH"] += ":" + os.path.dirname(__file__) + "/renode/bin"

    # Run unit tests with Renode.
    subprocess.run(
        [
            "test/run_device_tests.py",
            "-b",
            "bloonchipper",
            "--renode",
            "--with_private",
            "no",
        ],
        cwd=os.path.dirname(__file__),
        check=True,
    )


def main(args):
    """Builds, bundles, or tests all of the EC targets.

    Additionally, the tool reports build metrics.
    """
    opts = parse_args(args)

    if not hasattr(opts, "func"):
        print("Must select a valid sub command!")
        return -1

    # Run selected sub command function
    try:
        opts.func(opts)
    except subprocess.CalledProcessError:
        ec_dir = os.path.dirname(__file__)
        failed_dir = os.path.join(ec_dir, ".failedboards")
        if os.path.isdir(failed_dir):
            print("Failed boards/tests:")
            for fail in os.listdir(failed_dir):
                print(f"\t{fail}")
        return 1
    else:
        return 0


def parse_args(args):
    """Parse all command line args and return opts dict."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "--cpus",
        default=multiprocessing.cpu_count(),
        help="The number of cores to use.",
    )

    parser.add_argument(
        "--metrics",
        dest="metrics",
        required=True,
        help="File to write the json-encoded MetricsList proto message.",
    )

    parser.add_argument(
        "--metadata",
        required=False,
        help="Full pathname for the file in which to write build artifact metadata.",
    )

    parser.add_argument(
        "--output-dir",
        required=False,
        help="Full pathanme for the directory in which to bundle build artifacts.",
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
        help="Creates a tarball containing build artifacts from all firmware targets",
    )
    build_cmd.set_defaults(func=bundle)

    test_cmd = sub_cmds.add_parser("test", help="Runs all firmware unit tests")
    test_cmd.set_defaults(func=test)

    return parser.parse_args(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

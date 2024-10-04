#!/usr/bin/env -S python3 -u
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run unit tests on Renode emulator.

This is the entry point for the custom firmware builder workflow recipe.  It
gets invoked by chromite/api/controller/firmware.py.
"""

import argparse
import multiprocessing
from multiprocessing.pool import ThreadPool
import os
from pathlib import Path
import subprocess
import sys

# pylint: disable=import-error
from google.protobuf import json_format

from chromite.api.gen_sdk.chromite.api import firmware_pb2


def build(opts):
    """Build all the EC unit tests."""

    working_dir = Path(__file__).parents[2].resolve()
    cmd = [
        "make",
        "BOARD=bloonchipper",
        "tests",
        f"-j{opts.cpus}",
    ]
    return subprocess.run(cmd, cwd=working_dir).returncode == 0


def bundle(opts):
    """No-op."""

    # We don't produce any artifacts, but the info file is expected, so create
    # an empty one.
    with open(opts.metadata, "w", encoding="utf-8") as file:
        file.write(
            json_format.MessageToJson(
                firmware_pb2.FirmwareArtifactInfo()  # pylint: disable=no-member
            )
        )

    return True


def run_device_tests(test_name, working_dir):
    """Run the device tests."""

    return subprocess.run(
        [
            "test/run_device_tests.py",
            "-b",
            "bloonchipper",
            "--renode",
            "-t",
            test_name,
        ],
        cwd=working_dir,
        # TODO: check for failure
        # check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def test(opts):
    """Runs EC unit tests with Renode."""

    working_dir = Path(__file__).parents[2].resolve()
    renode_install_dir = working_dir.joinpath("renode")

    # Renode is built as a subtool and available versions can be found here:
    # https://chrome-infra-packages.appspot.com/p/chromiumos/infra/tools/renode.
    cipd_renode_version = (
        "ebuild_source:"
        + "app-crypt/mit-krb5-1.21.3,"
        + "app-emulation/renode-1.15.1_p20240714-r1,"
        + "dev-lang/mono-6.12.0.122,"
        + "sys-fs/e2fsprogs-1.47.0-r4,"
        + "sys-libs/glibc-2.37-r9"
    )

    # Install Renode.
    subprocess.run(
        [
            "cipd",
            "ensure",
            "-ensure-file",
            "-",
            "-root",
            renode_install_dir,
        ],
        input=("chromiumos/infra/tools/renode " + cipd_renode_version).encode(
            "utf-8"
        ),
        cwd=working_dir,
        check=True,
    )

    os.environ["PATH"] += ":" + str(renode_install_dir.joinpath("bin"))

    # Run unit tests with Renode.
    tests = [
        "production_app_test",
        "abort",
        "aes",
        "always_memset",
        "assert_builtin",
        "assert_stdlib",
        "benchmark",
        "boringssl_crypto",
        "cortexm_fpu",
        "crc",
        "exception",
        "exit",
        "flash_physical",
        "flash_write_protect",
        "fp_transport_spi_ro",
        "fp_transport_spi_rw",
        "fp_transport_uart_ro",
        "fp_transport_uart_rw",
        "fpsensor_auth_crypto_stateful",
        "fpsensor_auth_crypto_stateless",
        "fpsensor_crypto",
        "fpsensor_debug",
        "fpsensor_hw",
        "fpsensor_utils",
        "ftrapv",
        "libc_printf",
        "global_initialization",
        "libcxx",
        "malloc",
        "mpu_ro",
        "mpu_rw",
        "mutex",
        "mutex_trylock",
        "mutex_recursive",
        "panic",
        "pingpong",
        "printf",
        "queue",
        "restricted_console",
        "rng_benchmark",
        "rollback_region0",
        "rollback_region1",
        "rollback_entropy",
        "rtc",
        "rtc_stm32f4",
        "sbrk",
        "sha256",
        "sha256_unrolled",
        "static_if",
        "stdlib",
        "std_vector",
        "system_is_locked_wp_on",
        "system_is_locked_wp_off",
        "timer",
        "timer_dos",
        "tpm_seed_clear",
        "uart",
        "unaligned_access",
        "unaligned_access_benchmark",
        "utils",
        "utils_str",
        "power_utilization_idle",
        "power_utilization_sleep",
        "unaligned_access_bloonchipper_v2.0.4277",
        "unaligned_access_bloonchipper_v2.0.5938",
        "panic_data_bloonchipper_v2.0.4277",
        "panic_data_bloonchipper_v2.0.5938",
    ]

    ret = True
    pool = ThreadPool(20)
    results = []
    for t in tests:
        results.append(pool.apply_async(run_device_tests, (t, working_dir)))

    for r in results:
        test_result = r.get()
        print("%s" % test_result.stdout.decode("utf-8"))
        if test_result.returncode != 0:
            ret = False

    pool.close()
    pool.join()

    return ret


def main(args):
    """Builds, bundles, or tests.

    Additionally, the tool reports build metrics.
    """
    opts = parse_args(args)

    if not hasattr(opts, "func"):
        print("Must select a valid sub command!")
        return -1

    # Run selected sub command function
    if opts.func(opts):
        return 0

    return 1


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
        # required=True,
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

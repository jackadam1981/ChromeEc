#!/usr/bin/env python3

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script is a wrapper for invoking Twister, the Zephyr test runner, using
default parameters for the ChromiumOS EC project. For an overview of CLI
parameters that may be used, please consult the Twister documentation.
"""

import argparse
import os
import subprocess
from pathlib import Path


def main():
    """Run Twister using defaults for the EC project."""

    # Determine where the source tree is checked out.
    cros_checkout = os.environ.get("CROS_WORKON_SRCROOT")
    if not cros_checkout:
        raise RuntimeError(
            "Cannot find environment variable CROS_WORKON_SRCROOT."
            "Running outside of the chroot is not currently supported."
        )
    cros_checkout = Path(cros_checkout)

    # Use ZEPHYR_BASE environment var, or compute from checkout path if not
    # specified.
    zephyr_base = Path(
        os.environ.get(
            "ZEPHYR_BASE", cros_checkout / "src" / "third_party" / "zephyr" / "main"
        )
    )

    ec_base = cros_checkout / "src" / "platform" / "ec"

    # Module paths, including third party modules and the EC application.
    zephyr_modules = [
        cros_checkout / "src" / "third_party" / "zephyr" / "hal_stm32",
        cros_checkout / "src" / "third_party" / "zephyr" / "cmsis",
        cros_checkout / "src" / "third_party" / "zephyr" / "nanopb",
        ec_base,
    ]

    # Prepare environment variables for export to Twister and inherit the
    # parent environment.
    twister_env = dict(os.environ)
    twister_env.update(
        {
            "ZEPHYR_BASE": str(zephyr_base),
            "TOOLCHAIN_ROOT": str(cros_checkout / "src" / "platform" / "ec" / "zephyr"),
            "ZEPHYR_TOOLCHAIN_VARIANT": "llvm",
        }
    )

    # Twister CLI args
    twister_cli = [
        str(zephyr_base / "scripts" / "twister"),  # Executable path
        "--ninja",
        f"-x=DTS_ROOT={str( ec_base / 'zephyr')}",
        f"-x=SYSCALL_INCLUDE_DIRS={str(ec_base / 'zephyr' / 'include' / 'drivers')}",
        f"-x=ZEPHYR_MODULES={';'.join([str(p) for p in zephyr_modules])}",
    ]

    # `-T` flags (used for specifying test directories to build and run)
    # require special handling. When run without `-T` flags, Twister will
    # search for tests in `zephyr_base`. This is undesireable and we want
    # Twister to look in the EC tree by default, instead. Use argparse to
    # intercept `-T` flags and pass in a new default if none are found. If
    # user does pass their own `-T` flags, pass them through instead. Other
    # arguments get passed straight through.
    parser = argparse.ArgumentParser()
    parser.add_argument("-T", "--testsuite-root", action="append")
    t_args, other_args = parser.parse_known_args()

    if t_args.testsuite_root:
        # Pass user-provided -T args when present.
        for arg in t_args.testsuite_root:
            twister_cli.extend(["-T", arg])
    else:
        # Use EC base dir when no -T args specified. This will cause all
        # Twister-compatible EC tests to run.
        twister_cli.extend(["-T", str(ec_base)])

    # Append additional user-supplied args
    twister_cli.extend(other_args)

    # Invoke Twister and wait for it to exit.
    print("Invoking:", twister_cli)
    proc = subprocess.Popen(
        twister_cli,
        env=twister_env,
    )
    proc.wait()


if __name__ == "__main__":
    main()

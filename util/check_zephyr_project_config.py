#!/usr/bin/env python3

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Validate Zephyr project configuration files.
"""

import argparse
import logging
import os
import pathlib
import sys
import tempfile

ZEPHYR_BASE = os.environ.get("ZEPHYR_BASE")
if ZEPHYR_BASE is None:
    ZEPHYR_BASE = os.path.join(
        pathlib.Path(__file__).resolve().parents[3],
        "third_party",
        "zephyr",
        "main",
    )

sys.path.insert(0, os.path.join(ZEPHYR_BASE, "scripts"))
sys.path.insert(0, os.path.join(ZEPHYR_BASE, "scripts", "kconfig"))
# pylint:disable=import-error,wrong-import-position
import kconfiglib
import zephyr_module

# pylint:enable=import-error,wrong-import-position

# Known configuration file extensions.
CONF_FILE_EXT = (".conf", ".overlay", "_defconfig")


def _parse_args(argv):
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Verbose Output"
    )
    parser.add_argument(
        "-d",
        "--dt-has",
        action="store_false",
        help="Check for options that depends on a DT_HAS_..._ENABLE symbol.",
    )
    parser.add_argument(
        "CONFIG_FILE",
        nargs="+",
        help="List of config files to be checked, non config files are ignored.",
    )

    return parser.parse_args(argv)


def _init_kconfig():
    """Initialize a kconfiglib object with all boards and arch options."""
    with tempfile.TemporaryDirectory() as temp_dir:
        modules = zephyr_module.parse_modules(ZEPHYR_BASE, extra_modules=["."])

        kconfig = ""
        for module in modules:
            kconfig += zephyr_module.process_kconfig(
                module.project, module.meta
            )

        # generate Kconfig.modules file
        with open(pathlib.Path(temp_dir) / "Kconfig.modules", "w") as file:
            file.write(kconfig)

        # generate empty Kconfig.dts file
        with open(pathlib.Path(temp_dir) / "Kconfig.dts", "w") as file:
            file.write("")

        os.environ["ZEPHYR_BASE"] = ZEPHYR_BASE
        os.environ["srctree"] = ZEPHYR_BASE
        os.environ["KCONFIG_BINARY_DIR"] = temp_dir
        os.environ["ARCH_DIR"] = "arch"
        os.environ["ARCH"] = "*"
        os.environ["BOARD_DIR"] = "boards/*/*"

        return kconfiglib.Kconfig(os.path.join(ZEPHYR_BASE, "Kconfig"))


class KconfigCheck:
    """Validate Zephyr project configuration files.

    Attributes:
        argv: command line arguments
    """

    def __init__(self, argv):
        self.args = _parse_args(argv)
        self.log = self._init_log()
        self.kconf = _init_kconfig()
        self.fail_count = 0

    def _init_log(self):
        """Initialize a logger for the class."""
        console = logging.StreamHandler()
        console.setFormatter(logging.Formatter("%(levelname)s: %(message)s"))

        log = logging.getLogger(__file__)
        log.addHandler(console)

        if self.args.verbose:
            log.setLevel(logging.DEBUG)

        return log

    def _fail(self, *args):
        """Report a fail in the error log and incremenet the fail counter."""
        self.fail_count += 1
        self.log.error(*args)

    def _filter_config_files(self):
        """Yield files with known config suffixes from the command line."""
        for file in self.args.CONFIG_FILE:
            if file.endswith(CONF_FILE_EXT):
                yield file
            else:
                self.log.info("Ignoring %s: unrecognized suffix", file)

    def _check_dt_has(self, file_name):
        """Check file_name for known automatic config options."""
        symbols = {}
        for name, val in self.kconf.syms.items():
            dep = kconfiglib.expr_str(val.direct_dep)
            if "DT_HAS_" in dep:
                symbols[name] = dep

        self.log.info("Checking %s", file_name)

        with open(file_name, "r") as file:
            line_nr = 0
            for line in file.readlines():
                line_nr += 1
                for name in symbols:
                    match = f"CONFIG_{name}=y"
                    if line.startswith(match):
                        dep = symbols[name]
                        self._fail(
                            "Unnecessary config option %s found in %s:%d (depends on %s)",
                            match,
                            file_name,
                            line_nr,
                            dep,
                        )

    def run_checks(self):
        """Run all config checks."""
        config_files = list(self._filter_config_files())

        for file in config_files:
            if self.args.dt_has:
                self._check_dt_has(file)

        return self.fail_count


def main(argv):
    """Main function"""
    kconfig_checker = KconfigCheck(argv)
    return kconfig_checker.run_checks()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

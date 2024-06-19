# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configure-time replaces usb configuration string descriptor."""

import argparse
import logging
from pathlib import Path
import sys
from typing import List, Optional

import zmake.util
import zmake.version


def main(argv: Optional[List[str]] = None):
    """The main function.

    Args:
        argv: Optionally, the command-line to parse, not including argv[0].
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="EC build directory",
        required=True,
    )
    parser.add_argument(
        "--version-string",
        help="EC version string",
        required=True,
    )

    log_level_map = {
        "DEBUG": logging.DEBUG,
        "INFO": logging.INFO,
        "WARNING": logging.WARNING,
        "ERROR": logging.ERROR,
        "CRITICAL": logging.CRITICAL,
    }

    parser.add_argument(
        "-l",
        "--log-level",
        choices=log_level_map.values(),
        metavar=f"{{{','.join(log_level_map)}}}",
        type=lambda x: log_level_map[x],
        default=logging.INFO,
        help="Set the logging level (default=INFO)",
    )
    args = parser.parse_args(argv)

    log_format = "%(levelname)s: %(message)s"

    logging.basicConfig(format=log_format, level=args.log_level)

    version = '"' + args.version_string + '"'

    autoconf_path = (
        args.build_dir / "zephyr" / "include" / "generated" / "zephyr"
    )
    key = "CONFIG_USB_CONFIGURATION_STRING_DESC"

    value = zmake.util.read_kconfig_autoconf_value(autoconf_path, key)
    if value != '"EC_FIRMWARE_VERSION"':
        # No need to change usb configuration string descriptor
        return

    zmake.util.update_kconfig_autoconf_value(autoconf_path, key, version)

    logging.info("Replace USB configuration string descriptor with %s", version)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

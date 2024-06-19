import io
import logging
import re
import os, stat
from pathlib import Path
import sys

import argparse
import zmake.project
import zmake.version
import zmake.util
import scripts.util
from typing import List, Optional

def replace(inp, replace, replacewith):
    match = re.search(replace, inp)
    matches = []
    named_matches = {}
    if not match:
        return inp
    if match.group(0):
        matches = [match.group(0)]
    if match.groups():
        matches = matches + list(match.groups())
    named_matches = match.groupdict()
    if matches:
        try:
            replacewith = replacewith.format(*matches, **named_matches)
        except:
            pass
        return re.sub(replace, replacewith, inp)
    return inp

def write_configuration_setting(version_str, path):
    value = zmake.util.read_kconfig_autoconf_value(path, "CONFIG_USB_DEVICE_CONFIGURATION")
    if value != "\"EC_FIRMWARE_VERSION\"":
        # No need to change usb configuration string
        return
    if value == version_str:
        return

    autoconf_file = (path / "autoconf.h").resolve()
    inf = open(autoconf_file, "r")
    output = io.StringIO()
    version_string = "#define CONFIG_USB_DEVICE_CONFIGURATION " + str(value)
    line = inf.readline()
    while line:
        r = replace(line, version_string, "")
        output.write(r)
        line = inf.readline()
    inf.close()

    def add_def(name, value):
        output.write(f"#define {name} {zmake.util.c_str(value)}\n")
    add_def("CONFIG_USB_DEVICE_CONFIGURATION", version_str)

    with open(autoconf_file, "w") as f:
            f.write(output.getvalue())

    logging.info("Replace USB configuration string with %s", version_str)

def main(argv: Optional[List[str]] = None) -> Optional[int]:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="EC build directory",
        required=True,
    )
    parser.add_argument(
        "--project-dir",
        type=Path,
        help="EC project directory",
        required=True,
    )
    parser.add_argument(
        "--project-name",
        help="EC project name",
        required=True,
    )
    parser.add_argument(
        "--version",
        help="EC version string",
        required=True,
    )
    parser.add_argument(
        "--static-version",
        type=int,
        help="Static version flag",
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
    args = parser.parse_args()

    log_format = "%(levelname)s: %(message)s"

    logging.basicConfig(format=log_format, level=args.log_level)

    git_path = (args.project_dir / args.project_name).resolve()
    if args.version == "None":
        ec_version = "0.0.0"
    else:
        ec_version = args.version

    version_string = zmake.version.get_version_string(
        args.project_name,
        ec_version,
        static=args.static_version,
        git_path=git_path,
    )

    write_configuration_setting(
        version_string,
        args.build_dir / "zephyr" / "include" / "generated" / "zephyr"
        )

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

import io
import logging
import re
import os, stat
from pathlib import Path

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

def write_configuration_setting(version_str, filename):
    inf = open(filename, "r")
    output = io.StringIO()
    line = inf.readline()
    while line:
        ## TODO: need to use split
        r = replace(line, "#define CONFIG_USB_DEVICE_CONFIGURATION \"Test\"", "")

        output.write(r)
        line = inf.readline()
    inf.close()

    def add_def(name, value):
        output.write(f"#define {name} {zmake.util.c_str(value)}\n")
    add_def("CONFIG_USB_DEVICE_CONFIGURATION", version_str)

    with open(filename, "w") as f:
            f.write(output.getvalue())

def main(argv: Optional[List[str]] = None) -> Optional[int]:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="Path to Zephyr OS repository",
        required=True,
    )
    parser.add_argument(
        "--project-dir",
        type=Path,
        help="Path to Zephyr OS repository",
        required=True,
    )
    parser.add_argument(
        "--project-name",
        help="Path to Zephyr OS repository",
        required=True,
    )
    args = parser.parse_args()

    git_path = (args.project_dir / args.project_name).resolve()
    ## TODO: how to get version
    version_string = zmake.version.get_version_string(
        args.project_name,
        None,
        static=False,
        git_path=git_path,
    )

    write_configuration_setting(
        version_string,
        args.build_dir / "zephyr" / "include" / "generated" / "zephyr" / "autoconf.h"
        )

if __name__ == "__main__":
    main()

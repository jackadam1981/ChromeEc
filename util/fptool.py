#!/usr/bin/env python3
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to manage the fingerprint system on ChromeOS."""

import argparse
import pathlib
import shlex
import shutil
import subprocess
import sys
from typing import List, Optional


def util_exists(util_path: pathlib.Path) -> bool:
    """Returns True if the utility can be found, False otherwise."""
    if shutil.which(util_path):
        return True
    return False


def run(cmd: List[str]) -> int:
    """Run a command locally with the command echoed first."""
    print(f"> {shlex.join(cmd)}")
    sys.stdout.flush()
    p = subprocess.run(cmd, check=False)
    if p.returncode:
        print(f"Returned error status {p.returncode}.", file=sys.stderr)
    return p.returncode


def cmd_flash(opts: argparse.Namespace) -> int:
    """Flash the entire firmware FPMCU using the native bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.

    Args:
        opts:
            The argparse.Namespace that includes the optional `image` parameter,
            which is the path to the firmware image file.

    Returns:
        0 on success, non-zero upon error.
    """

    image: Optional[pathlib.Path] = opts.image

    FLASHER = pathlib.Path("flash_fp_mcu")

    if not util_exists(FLASHER):
        print(
            f"Error - The {FLASHER} utility does not exist.",
            file=sys.stderr,
        )
        return 1

    cmd = [str(FLASHER)]
    if image:
        if not image.is_file():
            print(f"Error - image {image} is not a file.", file=sys.stderr)
            return 1
        cmd.append(str(image))

    return run(cmd)


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    def brief(doc: Optional[str]) -> Optional[str]:
        if doc is None:
            return None
        return doc.split("\n")[0]

    parser = argparse.ArgumentParser(description=brief(__doc__))
    subparsers = parser.add_subparsers(dest="subcommand", title="subcommands")
    # This method of setting required is more compatible with older python.
    subparsers.required = True

    # Parser for "flash" subcommand.
    parser_flash = subparsers.add_parser("flash", help=brief(cmd_flash.__doc__))
    parser_flash.add_argument(
        "image", nargs="?", type=pathlib.Path, help="Path to the firmware image"
    )
    parser_flash.set_defaults(func=cmd_flash)
    opts = parser.parse_args(argv)
    return opts.func(opts)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

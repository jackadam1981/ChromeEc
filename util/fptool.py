#!/usr/bin/env python3
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to manage the fingerprint system on ChromeOS."""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
from typing import List


DISABLE_UPDATER_FILE = pathlib.Path(
    "/mnt/stateful_partition/.disable_fp_updater"
)


def run(cmd: List[str]) -> int:
    """Run a command locally with the command echoed first."""
    print(f'> {" ".join(cmd)}')
    sys.stdout.flush()
    p = subprocess.run(cmd)  # pylint: disable=subprocess-run-check
    if p.returncode:
        print(f"Returned error status {p.returncode}.")
    return p.returncode


def remote_run(host: str, port: int, remote_cmd: List[str]) -> int:
    """Run a command on a remote machine using ssh."""
    cmd = ["ssh", f"-p{port}", host, "--"] + remote_cmd
    return run(cmd)


def cmd_flash(args: argparse.Namespace) -> int:
    """Flash the entire firmware FPMCU using the native bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """

    if not shutil.which("flash_fp_mcu"):
        print("Error - The flash_fp_mcu utility does not exist.")
        return 1

    cmd = ["flash_fp_mcu"]
    if args.image:
        if not os.path.isfile(args.image):
            print(f"Error - image {args.image} is not a file.")
            return 1
        cmd.append(args.image)

    print(f'Running {" ".join(cmd)}.')
    sys.stdout.flush()
    p = subprocess.run(cmd)  # pylint: disable=subprocess-run-check
    return p.returncode


def updater_disable(disable: bool) -> int:
    """Disable or enable the fingerprint updater."""
    if disable:
        print("# Disabling fingerprint updater.")
        action = "touch"
    else:
        print("# Enabling fingerprint updater.")
        action = "rm"

    ret = run([action, str(DISABLE_UPDATER_FILE)])
    if ret:
        print(f"Failed to {action} {DISABLE_UPDATER_FILE}.")
        return ret

    ret = run(["sync"])
    if ret:
        print(f"Failed to sync {DISABLE_UPDATER_FILE}.")
        return ret

    return 0


def cmd_updater_enable(args: argparse.Namespace) -> int:
    """Enable fingerprint firmware updater."""
    return updater_disable(False)


def cmd_updater_disable(args: argparse.Namespace) -> int:
    """Disable fingerprint firmware updater."""
    return updater_disable(True)


def remote_updater_disable(host: str, port: int, disable: bool) -> int:
    """Disable or enable fingerprint firmware updater on remote DUT."""

    if disable:
        print("# Disabling fingerprint updater.")
        action = "touch"
    else:
        print("# Enabling fingerprint updater.")
        action = "rm"

    ret = remote_run(
        host, port, [action, str(DISABLE_UPDATER_FILE), ";", "sync"]
    )
    if ret:
        print(f"Failed to {action} {DISABLE_UPDATER_FILE}.")
        return ret

    return 0


def cmd_remote_updater_enable(args: argparse.Namespace) -> int:
    """Enable fingerprint firmware updater on remote DUT."""
    return remote_updater_disable(args.host, args.port, False)


def cmd_remote_updater_disable(args: argparse.Namespace) -> int:
    """Disable fingerprint firmware updater on remote DUT."""
    return remote_updater_disable(args.host, args.port, True)


def cmd_remote_flash(args: argparse.Namespace) -> int:
    """Remotely flash the entire firmware FPMCU using the native bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """

    REMOTE_TMP_IMG = "/tmp/fpmcu-fw.bin"

    host = args.host
    port = args.port
    image = args.image

    image_name = image if image else "default rootfs FW"
    print(f"Flashing {image_name} to {host}:{port}.")

    if image:
        print(f"# Copying image {image} to {host}.")
        scp_cmd = ["scp", f"-P{port}", str(image), f"{host}:{REMOTE_TMP_IMG}"]
        ret = run(scp_cmd)
        if ret:
            print("Failed to copy image to host.")
            return ret

    print("# Flashing copied image.")
    remote_flash_cmd = ["fptool", "flash"]
    if image:
        remote_flash_cmd += [REMOTE_TMP_IMG]
    ret = remote_run(host, port, remote_flash_cmd)
    if ret:
        print("Failed to flash FPMCU.")
        return ret

    return 0


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="subcommand", title="subcommands")
    # This method of setting required is more compatible with older python.
    subparsers.required = True

    # Parser for "flash" subcommand.
    parser_flash = subparsers.add_parser("flash", help=cmd_flash.__doc__)
    parser_flash.add_argument(
        "image", nargs="?", help="Path to the firmware image"
    )
    parser_flash.set_defaults(func=cmd_flash)

    # Parser for "updater-enable" subcommand.
    subparsers.add_parser(
        "updater-enable", help=cmd_updater_enable.__doc__
    ).set_defaults(func=cmd_updater_enable)

    # Parser for "updater-disable" subcommand.
    subparsers.add_parser(
        "updater-disable", help=cmd_updater_disable.__doc__
    ).set_defaults(func=cmd_updater_disable)

    # Parser with subcommands for "remote [options] <host> <subcommand>".
    parser_remote = subparsers.add_parser(
        "remote",
        help="Commands for working with the fingerprint subsystem on a remote DUT.",
    )
    parser_remote.add_argument(
        "--port", default=22, type=int, help="Target SSH port number."
    )
    parser_remote.add_argument("host", help="Target SSH hostname.")
    subparsers_remote = parser_remote.add_subparsers(
        dest="remote_subcommand", title="remote_subcommands"
    )
    subparsers_remote.required = True

    # Parser for "remote flash <image>" subcommand.
    parser_remote_flash = subparsers_remote.add_parser(
        "flash", help=cmd_remote_flash.__doc__
    )
    parser_remote_flash.add_argument(
        "image", nargs="?", type=pathlib.Path, help="Path to the firmware image"
    )
    parser_remote_flash.set_defaults(func=cmd_remote_flash)

    # Parser for "remote updater-enable" subcommand.
    subparsers_remote.add_parser(
        "updater-enable", help=cmd_remote_updater_enable.__doc__
    ).set_defaults(func=cmd_remote_updater_enable)

    # Parser for "remote updater-disable" subcommand.
    subparsers_remote.add_parser(
        "updater-disable", help=cmd_remote_updater_disable.__doc__
    ).set_defaults(func=cmd_remote_updater_disable)

    opts = parser.parse_args(argv)
    return opts.func(opts)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

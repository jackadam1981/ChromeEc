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


UPDATER_DISABLE_FILE = pathlib.Path(
    "/mnt/stateful_partition/.disable_fp_updater"
)


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


def remote_run(host: str, port: int, remote_cmd: List[str]) -> int:
    """Run a command on a remote machine using ssh."""
    cmd = ["ssh", f"-p{port}", host, "--"] + remote_cmd
    return run(cmd)


def scp_file(host: str, port: int, src: pathlib.Path, dst: pathlib.Path) -> int:
    """Copy one file to remote host over scp."""
    scp_cmd = ["scp", f"-P{port}", src, f"{host}:{dst}"]
    return run(scp_cmd)


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


def updater_disable(disable: bool) -> int:
    """Disable or re-enable the fingerprint updater.

    Args:
        disable:
            When True, disable auto updates.
            When False, resume auto updates.

    Returns:
        0 on success, non-zero upon error.
    """

    if disable:
        print("# Disabling fingerprint updater.")
        action = "touch"
    else:
        print("# Enabling fingerprint updater.")
        action = "rm"

    ret = run([action, str(UPDATER_DISABLE_FILE)])
    if ret:
        print(f"Failed to {action} {UPDATER_DISABLE_FILE}.", file=sys.stderr)
        return ret

    ret = run(["sync"])
    if ret:
        print(f"Failed to sync {UPDATER_DISABLE_FILE}.", file=sys.stderr)
        return ret

    return 0


def cmd_updater_enable(opts: argparse.Namespace) -> int:
    """Enable fingerprint firmware updater."""
    return updater_disable(False)


def cmd_updater_disable(opts: argparse.Namespace) -> int:
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
        host, port, [action, str(UPDATER_DISABLE_FILE), ";", "sync"]
    )
    if ret:
        print(f"Failed to {action} {UPDATER_DISABLE_FILE}.", file=sys.stderr)
        return ret

    return 0


def cmd_remote_updater_enable(opts: argparse.Namespace) -> int:
    """Enable fingerprint firmware updater on remote DUT."""
    return remote_updater_disable(opts.host, opts.port, False)


def cmd_remote_updater_disable(opts: argparse.Namespace) -> int:
    """Disable fingerprint firmware updater on remote DUT."""
    return remote_updater_disable(opts.host, opts.port, True)


def cmd_remote_flash(opts: argparse.Namespace) -> int:
    """Remotely flash the entire firmware FPMCU using the native bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """

    REMOTE_TMP_IMG = pathlib.Path("/tmp/fpmcu-fw.bin")

    host: str = opts.host
    port: int = opts.port
    image: Optional[pathlib.Path] = opts.image

    image_name = image if image else "default rootfs FW"
    print(f"Flashing {image_name} to {host}:{port}.")

    if image:
        print(f"# Copying image {image} to {host}.")
        ret = scp_file(host, port, image, REMOTE_TMP_IMG)
        if ret:
            print("Failed to copy image to host.", file=sys.stderr)
            return ret

    print("# Flashing copied image.")
    remote_flash_cmd = ["fptool", "flash"]
    if image:
        remote_flash_cmd += [str(REMOTE_TMP_IMG)]
    ret = remote_run(host, port, remote_flash_cmd)
    if ret:
        print("Failed to flash FPMCU.", file=sys.stderr)
        return ret

    return 0


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
        help="Commands for working with the fingerprint subsystem on a remote "
        "DUT.",
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

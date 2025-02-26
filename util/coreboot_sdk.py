#!/usr/bin/env -S python3 -u
# -*- coding: utf-8 -*-
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide coreboot-sdk to callers

Initialize the coreboot-sdk subtools and provide environment variables
to the caller to indicate the extracted location.
"""

import argparse
import os
import shutil
import subprocess
from typing import Dict, List, Optional, Tuple, Union
import urllib.request

from coreboot_sdk_portage_deps import get_portage_deps


toolchain_name_map = {
    "arm-eabi": "COREBOOT_SDK_ROOT_arm",
    "picolibc-arm-eabi": "COREBOOT_SDK_ROOT_picolibc_arm",
    "libstdcxx-arm-eabi": "COREBOOT_SDK_ROOT_libstdcxx_arm",
    "i386-elf": "COREBOOT_SDK_ROOT_x86",
    "picolibc-i386-elf": "COREBOOT_SDK_ROOT_picolibc_x86",
    "libstdcxx-i386-elf": "COREBOOT_SDK_ROOT_libstdcxx_x86",
    "riscv-elf": "COREBOOT_SDK_ROOT_riscv",
    "nds32le-elf": "COREBOOT_SDK_ROOT_nds32",
}


def get_toolchains_bazel(
    portage_toolchains: Dict[str, Tuple]
) -> Dict[str, str]:
    """Download and extract the toolchains using bazel.

    Args:
        portage_toolchains: Dict of architectures to download

    Returns:
        Dict of coreboot-sdk env variables and their respective paths
    """
    subprocess.run(
        [
            "bazel",
            "--project",
            "fwsdk",
            "build",
            *(
                f"@ec-coreboot-sdk-{target}//:get_path"
                for target, _ in portage_toolchains.items()
            ),
        ],
        check=True,
        cwd="/mnt/host/source/src/",
    )

    result = {}
    for target, _ in portage_toolchains.items():
        run_result = subprocess.run(
            [
                "bazel",
                "--project",
                "fwsdk",
                "run",
                f"@ec-coreboot-sdk-{target}//:get_path",
            ],
            check=True,
            stdout=subprocess.PIPE,
            cwd="/mnt/host/source/src/",
        )
        result[
            toolchain_name_map[target] or f"COREBOOT_SDK_ROOT_{target}"
        ] = run_result.stdout.strip().decode("utf-8")
    return result


def get_toolchains_shell(
    portage_toolchains: Dict[str, Tuple],
    local_filepath: Union[str, "os.PathLike[str]"] = os.path.expanduser(
        "~/.cache/coreboot-sdk"
    ),
) -> Dict[str, str]:
    """Download and extract the toolchains using the shell.

    Args:
        portage_toolchains: Dict of architectures to download
        local_filepath: Path to download the toolchains to

    Returns:
        Dict of coreboot-sdk env variables and their respective paths
    """
    result = {}
    for target, (version, toolchain_hash) in portage_toolchains.items():
        # TODO JPM support overrides
        output_path = local_filepath + "/" + target + "/" + toolchain_hash
        downloaded_file = output_path + ".tar.zst"
        if os.path.exists(output_path) and os.path.isdir(output_path):
            print(f"Skipping {downloaded_file} because the output dir exists")
            continue
        src_uri = (
            "https://storage.googleapis.com/chromiumos-sdk/toolchains/coreboot-sdk"
            f"-{target}/{version}/{toolchain_hash}.tar.zst"
        )
        try:
            print("making dir " + downloaded_file)
            os.makedirs(
                output_path, exist_ok=True
            )  # Create parent directories if needed

            with urllib.request.urlopen(src_uri) as source_file, open(
                downloaded_file, "wb"
            ) as dest_file:
                shutil.copyfileobj(source_file, dest_file)
        except urllib.error.HTTPError as e:
            print(f"HTTP Error: {e.code} - {e.reason}")
            continue
        except TimeoutError as e:
            print(f"Timeout Error: {e}")
            continue
        except ConnectionRefusedError as e:
            print(f"Connection refused: {e}")
            continue
        except urllib.error.URLError as e:
            print(f"URL Error: {e.reason}")
            continue
        except shutil.SameFileError:
            print("The source and dest files are the same")
            continue
        except FileNotFoundError:
            print(f"Error: File not found '{downloaded_file}'")
            continue
        except OSError as e:
            print(f"OS Error: {e}")
            continue

        try:
            subprocess.run(
                ["tar", "-xf", downloaded_file, "-C", output_path],
                check=True,
            )
            print(
                f"Successfully extracted '{downloaded_file}'"
                f" using built-in zstd support to '{output_path}'"
            )
        except subprocess.CalledProcessError as e:
            print(f"Error extracting with command-line tool: {e}")
            continue

        result[
            toolchain_name_map.get(target) or f"COREBOOT_SDK_ROOT_{target}"
        ] = output_path
    return result


def init_toolchain(use_shell: bool = False) -> Dict[str, str]:
    """Initialize coreboot-sdk.

    Returns:
        Environment variables to use for toolchain.
    """
    if os.environ.get("COREBOOT_SDK_ROOT") is not None:
        print("COREBOOT_SDK_ROOT already set by environment, returning")
        return {}

    if not use_shell:
        try:
            subprocess.run(
                [
                    "bazel",
                    "--project",
                    "fwsdk",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
                check=True,
                cwd="/mnt/host/source/src/",
            )
        except (subprocess.CalledProcessError, FileNotFoundError):
            print(
                "bazel doesn't exist or is not the right version to download packages"
                " for coreboot-sdk.  Attempting with the shell"
            )
            use_shell = True

    portage_toolchains = get_portage_deps()

    env_var_map = {}

    if use_shell:
        env_var_map = get_toolchains_shell(portage_toolchains)
    else:
        env_var_map = get_toolchains_bazel(portage_toolchains)

    return env_var_map


def main(argv: Optional[List[str]] = None):
    """Main calling function for the script"""
    parser = argparse.ArgumentParser(description="coreboot_sdk")
    parser.add_argument(
        "--shell",
        help="Don't push",
        action="store_true",
    )
    opts = parser.parse_args(argv)
    env_vars = init_toolchain(opts.shell)
    # Return a formatted string which can be declared as an associative array in bash
    if env_vars:
        print(
            " ".join(f'["{key}"]="{value}"' for key, value in env_vars.items())
        )


if __name__ == "__main__":
    main()

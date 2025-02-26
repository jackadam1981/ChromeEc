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
from coreboot_sdk_portage_deps import get_portage_deps
import os
import requests
import subprocess
import tarfile
from typing import Dict, List, Optional, Tuple, Union

toolchain_name_map = {
    'arm-eabi': 'COREBOOT_SDK_ROOT_arm',
    'picolibc-arm-eabi': 'COREBOOT_SDK_ROOT_picolibc_arm',
    'libstdcxx-arm-eabi': 'COREBOOT_SDK_ROOT_libstdcxx_arm',
    'i386-elf': 'COREBOOT_SDK_ROOT_x86',
    'picolibc-i386-elf': 'COREBOOT_SDK_ROOT_picolibc_x86',
    'libstdcxx-i386-elf': 'COREBOOT_SDK_ROOT_libstdcxx_x86',
    'riscv-elf': 'COREBOOT_SDK_ROOT_riscv',
    'nds32le-elf': 'COREBOOT_SDK_ROOT_nds32'
}

def get_toolchains_bazel(portage_toolchains: Dict[str, Tuple]) -> Dict[str, str]:
    subprocess.run(
        [
            "bazel",
            "--project",
            "fwsdk",
            "build",
            *(f"@ec-coreboot-sdk-{target}//:get_path" for target, _ in portage_toolchains.items()),
        ],
        check=True,
        cwd="/mnt/host/source/src/",
    )

    result = {}
    for target, _ in portage_toolchains.items():
        run_result = subprocess.run(
            ["bazel", "--project", "fwsdk", "run", f"@ec-coreboot-sdk-{target}//:get_path"],
            check=True,
            stdout=subprocess.PIPE,
            cwd="/mnt/host/source/src/",
        )
        result[toolchain_name_map[target] or f"COREBOOT_SDK_ROOT_{target}"] = run_result.stdout.strip().decode('utf-8')
    return result

def get_toolchains_shell(portage_toolchains: Dict[str, Tuple], local_filepath: Union[str, "os.PathLike[str]"] = "/tmp") -> Dict[str, str]:
    result = {}
    for target, (version, toolchain_hash) in portage_toolchains.items():
        # TODO JPM support overrides
        output_path = local_filepath + "/" + toolchain_hash
        downloaded_file = output_path + ".tar.zst"
        src_uri = "https://storage.googleapis.com/chromiumos-sdk/toolchains/coreboot-sdk-%s/%s/%s.tar.zst" % (target, version, toolchain_hash)
        try:
            response = requests.get(src_uri, stream=True)
            response.raise_for_status()
            print("making dir " + downloaded_file)
            os.makedirs(output_path, exist_ok=True)  # Create parent directories if needed

            with open(downloaded_file, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)
        except requests.exceptions.RequestException as e:
            print(f"Error downloading file: {e}, skipped")
            continue
        try:
            with tarfile.open(downloaded_file, 'r:zstd') as tar_file:
                tar_file.extractall(output_path)
            print(f"Successfully extracted '{downloaded_file}' using built-in zstd support to '{output_path}'")
        except tarfile.TarError as e:
            # If built-in support fails, try command-line tool
            print(f"Built-in zstd support failed: {e}. Trying command-line tool.")
            try:
                subprocess.run(['tar', '-xf', downloaded_file, '-C', output_path], check=True)
                print(f"Successfully extracted '{downloaded_file}' using command-line tool to '{output_path}'")
            except subprocess.CalledProcessError as e:
                print(f"Error extracting with command-line tool: {e}")
                continue
            except Exception as e:
                print(f"An unexpected error occurred with command-line tool: {e}")
                continue

        except FileNotFoundError:
            print(f"Error: File not found '{downloaded_file}'")
            continue
        except tarfile.TarError as e:
            print(f"Error extracting tar file: {e}")
            continue
        except Exception as e:
            print(f"An unexpected error occurred: {e}")
            continue
        result[toolchain_name_map.get(target) or f"COREBOOT_SDK_ROOT_{target}"] = output_path
    return result

def init_toolchain(opts: argparse.Namespace) -> Dict[str, str]:
    """Initialize coreboot-sdk.

    Returns:
        Environment variables to use for toolchain.
    """
    if os.environ.get("COREBOOT_SDK_ROOT") is not None:
        print("COREBOOT_SDK_ROOT already set by environment, returning")
        return {}

    if not opts or not opts.shell:
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
            opts.shell = True

    portage_toolchains = get_portage_deps(opts.eclass)

    env_var_map = {}

    if opts.shell:
        env_var_map = get_toolchains_shell(portage_toolchains)
    else:
        env_var_map = get_toolchains_bazel(portage_toolchains)

    return env_var_map


def main(argv: Optional[List[str]] = None):
    parser = argparse.ArgumentParser(description="coreboot_sdk")
    parser.add_argument(
        "--eclass",
        help="Eclass to parse and generate toolchains from",
        default="~/chromiumos/src/third_party/chromiumos-overlay/eclass/coreboot-sdk-ec-dependencies.eclass",
    )
    parser.add_argument(
        "--shell",
        help="Don't push",
        action="store_true",
    )
    opts = parser.parse_args(argv)
    env_vars = init_toolchain(opts)
    # Return a formatted string which can be declared as an associative array in bash
    if env_vars:
        print(
            " ".join(
                f"[\"{key}\"]=\"{value}\""
                for key, value in env_vars.items()
            )
        )

if __name__ == "__main__":
    main()
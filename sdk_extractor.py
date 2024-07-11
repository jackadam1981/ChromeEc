#!/usr/bin/env vpython3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SDK Extractor script.

This script extracts the specified coreboot-sdk.

Usage: sdk_extractor.py [options...]
"""

# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/requests-py3"
#   version: "version:2.31.0"
# >
# wheel: <
#   name: "infra/python/wheels/certifi-py2_py3"
#   version: "version:2020.11.8"
# >
# wheel: <
#   name: "infra/python/wheels/idna-py2_py3"
#   version: "version:2.8"
# >
# wheel: <
#   name: "infra/python/wheels/charset_normalizer-py3"
#   version: "version:2.0.4"
# >
# wheel: <
#   name: "infra/python/wheels/urllib3-py2_py3"
#   version: "version:1.26.6"
# >
# [VPYTHON:END]

import argparse
import logging
import os
import tempfile

import requests  # pylint: disable=import-error


logger = logging.getLogger(__name__)


def extract_sdk(args, tmp_dir):
    """Extract SDK.

    Args:
        args: The parsed command line arguments.
        tmp_dir: Temporary path to download sdk artifacts
    """
    coreboot_sdk_uri_path = (
        "https://storage.googleapis.com/chromiumos-sdk/toolchains/coreboot-sdk"
    )

    for sdk_toolchain in args.toolchain:
        toolchain, toolchain_version, toolchain_hash = sdk_toolchain.split(":")

        tarball = f"{toolchain_hash}.tar.zst"
        tmp_path = f"{tmp_dir}/{toolchain}/{toolchain_version}"
        tmp_file = f"{tmp_path}/{tarball}"
        install_path = f"{args.install_path}/{toolchain}"
        sdk_url = (
            f"{coreboot_sdk_uri_path}-{toolchain}/{toolchain_version}/{tarball}"
        )

        logger.info(
            "Downloading %s from %s with URL %s",
            tarball,
            coreboot_sdk_uri_path,
            sdk_url,
        )
        if not os.path.exists(tmp_path):
            os.makedirs(tmp_path)
        with open(tmp_file, "wb") as sdk_tarball:
            content = requests.get(sdk_url, stream=True).content
            sdk_tarball.write(content)

        if not os.path.exists(install_path):
            os.makedirs(install_path)
        os.system(
            f"tar -I pzstd --no-same-owner -xf {tmp_file} -C {install_path}"
        )


def main(argv=None):
    """The entry point to the program."""
    parser = argparse.ArgumentParser(description="SDK Extractor")
    parser.add_argument(
        "--toolchain",
        action="append",
        default=[],
        help="Toolchain to be extracted with version and hash separated"
        " by colons",
    )
    parser.add_argument(
        "--install-path",
        default="build/coreboot-sdk/",
        help="Installation path for the toolchains",
    )
    args = parser.parse_args(argv)
    logging.basicConfig(
        format="%(asctime)s %(levelname)s: %(message)s",
        level=logging.INFO,
    )
    if len(args.toolchain) == 0:
        logger.error("Must specify at least 1 toolchain")
    with tempfile.TemporaryDirectory(".corebootsdk") as tmp_dir:
        extract_sdk(args, tmp_dir)


if __name__ == "__main__":
    main()

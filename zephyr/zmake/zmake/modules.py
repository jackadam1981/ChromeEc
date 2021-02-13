# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Registry of known Zephyr modules."""

import pathlib
import os

import zmake.build_config as build_config
import zmake.util as util


def third_party_module(name, modules_dir, version):
    """Common callback in registry for all third_party/zephyr modules.

    Args:
        name: The name of the module.
        modules_dir: The path to the modules directory.
        version: The zephyr version.

    Return:
        The path to the module.
    """
    if not version or len(version) < 2:
        return None
    return modules_dir / name / 'v{}.{}'.format(version[0], version[1])


known_modules = ['hal_stm32', 'cmsis']


def locate_modules(modules_dir, ec_dir, version):
    """Resolve module locations from a known_modules dictionary.

    Args:
        modules_dir: The path to the modules directory
        ec_dir: The path to the EC directory (e.g. 'src/platform/ec')
        version: The zephyr version, as a two or three tuple of ints.

    Returns:
        A dictionary mapping module names to paths.
    """
    result = {}
    for name in known_modules:
        result[name] = third_party_module(name, modules_dir, version)
    result['ec-shim'] = ec_dir
    return result


def setup_module_symlinks(output_dir, modules):
    """Setup a directory with symlinks to modules.

    Args:
        output_dir: The directory to place the symlinks in.
        modules: A dictionary of module names mapping to paths.

    Returns:
        The resultant BuildConfig that should be applied to use each
        of these modules.
    """
    if not output_dir.exists():
        output_dir.mkdir(parents=True)

    module_links = []

    for name, path in modules.items():
        link_path = output_dir.resolve() / name
        util.update_symlink(path, link_path)
        module_links.append(link_path)

    if module_links:
        return build_config.BuildConfig(
            cmake_defs={'ZEPHYR_MODULES': ';'.join(map(str, module_links))})
    else:
        return build_config.BuildConfig()

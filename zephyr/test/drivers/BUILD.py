# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Construct the drivers test binaries"""

import pathlib

import yaml


default_config = {
    "dts_overlays": [
        here / "overlay.dts",
    ],
    "kconfig_files": [
        here / "prj.conf",
    ],
    "test_args": ["-flash={test_temp_dir}/flash.bin"],
}


def merge_dictionary(dict_1, dict_2):
    """Merge dict_1 and dict_2 and return the result"""
    dict_3 = {**dict_1, **dict_2}
    for key, value in dict_3.items():
        if key in dict_1 and key in dict_2:
            if isinstance(value, list) or isinstance(dict_1[key], list):
                dict_3[key] = value + dict_1[key]
            else:
                dict_3[key] = [value, dict_1[key]]
    return dict_3


def update_relative_path(project_root, path_list):
    """Update instances of '${here}' in path_list to project_root"""
    for index, path_item in enumerate(path_list):
        path_list[index] = pathlib.Path(path_item.replace("${here}", str(project_root)))


# Register subprojects
for path in pathlib.Path(here).rglob("TEST.yaml"):
    with open(path, "r") as stream:
        yaml_overlay = yaml.load(stream)
    config = dict(default_config)
    if yaml_overlay:
        if yaml_overlay["dts_overlays"]:
            update_relative_path(path.parent, yaml_overlay["dts_overlays"])
        if yaml_overlay["kconfig_files"]:
            update_relative_path(path.parent, yaml_overlay["kconfig_files"])
        config = merge_dictionary(config, yaml_overlay)
    if "-" in path.parent.name:
        raise ValueError(
            "Project directory ({}) may not contain '-'".format(path.parent.name)
        )
    PROJECT_NAME = "drivers-{}".format(path.parent.name)
    register_host_test(
        PROJECT_NAME,
        **config,
    )

register_host_test(
    "drivers",
    **default_config,
)

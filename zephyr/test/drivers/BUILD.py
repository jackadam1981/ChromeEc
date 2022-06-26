# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import yaml

"""Register zmake project for drivers test."""

default_config = {
        'dts_overlays': [
                here / "overlay.dts",
                # here / "led_driver/led_pins.dts",
                # here / "led_driver/led_policy.dts",
        ],
        'kconfig_files': [
                here / "prj.conf",
        ],
        'test_args': ["-flash={test_temp_dir}/flash.bin"],
}

def merge_dictionary(dict_1, dict_2):
   dict_3 = {**dict_1, **dict_2}
   for key, value in dict_3.items():
       if key in dict_1 and key in dict_2:
           if isinstance(value, list) or isinstance(dict_1[key], list):
               dict_3[key] = value + dict_1[key]
           else:
               dict_3[key] = [value, dict_1[key]]
   return dict_3

def update_relative_path(project_root, path_list):
    for i, path in enumerate(path_list):
        path_list[i] = pathlib.Path(path.replace("${here}", str(project_root)))

# Register subprojects
for path in pathlib.Path(here).rglob("TEST.yaml"):
    print("path={}".format(path))
    with open(path, 'r') as stream:
        yaml_overlay = yaml.load(stream)
    print("yaml_overlay={}".format(yaml_overlay))
    config = dict(default_config)
    if yaml_overlay:
        if yaml_overlay['dts_overlays']:
            update_relative_path(path.parent, yaml_overlay['dts_overlays'])
        if yaml_overlay['kconfig_files']:
            update_relative_path(path.parent, yaml_overlay['kconfig_files'])
        config = merge_dictionary(config, yaml_overlay)
    if (path.parent / "prj.conf").exists():
        print("FOUND {}".format(path.parent / "prj.conf"))
    print("config={}".format(config))
    project_name = "drivers-{}".format(path.parent.name)
    register_host_test(
            project_name,
            **config,
    )

# register the core project (until we can split it up)
register_host_test(
        "drivers",
        **default_config,
)

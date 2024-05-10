#!/usr/bin/env python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def modify_header_file(filepath):
    with open(filepath, "r") as file:
        lines = file.readlines()

    last_include_index = -1
    for i, line in enumerate(lines):
        if line.startswith('#include "') or line.startswith("#include <"):
            last_include_index = i
        if line.startswith("#ifdef __cplusplus"):
            return

    if last_include_index != -1:
        lines.insert(
            last_include_index + 1,
            '\n#ifdef __cplusplus\nextern "C" {\n#endif\n',
        )
        if lines[-1].startswith("#endif"):
            lines.insert(-2, "\n#ifdef __cplusplus\n}\n#endif\n")

    with open(filepath, "w") as file:
        file.writelines(lines)


def process_directory(directory):
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith(".h"):
                filepath = os.path.join(root, file)
                modify_header_file(filepath)


if __name__ == "__main__":
    process_directory("./include")

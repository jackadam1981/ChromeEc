#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build all of the EC boards.

This is the entry point for the custom firmware builder workflow recipe.
"""

import sys

from chromite.lib import cros_build_lib


def main():
    """Builds all of the EC targets"""
    result = cros_build_lib.run(
        ['make', 'buildall', '-j'],
        check=False,  # Don't throw exception on non-zero return code
        stdout=1,  # passthrough stdout
        stderr=1)  # passthrough stderr
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())

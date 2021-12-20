#!/usr/bin/env python3
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to manage the fingerprint system on Chrome OS."""

import argparse
import os
import shutil
import subprocess
import sys
import glob
import re
import datetime
import stat
import time

def cmd_flash(args: argparse.Namespace) -> int:
    """
    Flash the entire firmware FPMCU using the native bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """

def main() -> int:
    # print out canonical path to differentiate between /usr/local/bin and
    # /usr/bin installs
    run_system_cmd(f"readlink -f {sys.argv[0]}", show_output=True)
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='subcommand', title='subcommands')
    # This method of setting required is more compatible with older python.
    subparsers.required = True

    parser_decrypt = flash_init(subparsers)
    opts = parser.parse_args()

    return opts.func(opts)

if __name__ == '__main__':
    sys.exit(main())

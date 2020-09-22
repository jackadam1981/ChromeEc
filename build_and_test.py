#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build and test all of the EC boards.

This is the entry point for the custom firmware builder workflow recipe.
"""

import argparse
import multiprocessing
import os
import subprocess
import sys


def main(args):
    """Builds all of the EC targets"""
    opts = parse_args(args)

    result = subprocess.run(['make', 'buildall', '-j{}'.format(opts.cpus)],
                            cwd=os.path.dirname(__file__))
    return result.returncode


def parse_args(args):
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        '--cpus',
        dest='cpus',
        default=multiprocessing.cpu_count(),
        help='The number of cores to use.',
    )

    parser.add_argument(
        '--metric_file',
        dest='metric_file',
        help=
        'File to write a json-encoded BuildAllFirmwareResponse proto Message.',
    )

    return parser.parse_args(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

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


def _build(opts):
    """Builds all EC firmware targets"""
    return subprocess.run(['make', 'buildall_only', '-j{}'.format(opts.cpus)],
                          cwd=os.path.dirname(__file__)).returncode


def _test(opts):
    """Runs all of the unit tests for EC firmware"""
    return subprocess.run(['make', 'runtests', '-j{}'.format(opts.cpus)],
                          cwd=os.path.dirname(__file__)).returncode


def _metrics(opts):
    """Writes out all of the firmware build metrics to a file"""
    # TODO(jettrink): Add metrics output. Just touch file for now
    return subprocess.run(['touch', '{}'.format(opts.file)],
                          cwd=os.path.dirname(__file__)).returncode


def main(args):
    """Builds and tests all of the EC targets and reports build metrics"""
    opts = parse_args(args)

    if opts.sub == 'build':
        return _build(opts)
    elif opts.sub == 'test':
        return _test(opts)
    elif opts.sub == 'metrics':
        return _metrics(opts)
    else:
        print("Must select a valid sub command!")
        return -1


def parse_args(args):
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        '--cpus',
        default=multiprocessing.cpu_count(),
        help='The number of cores to use.',
    )

    # Would make this required=True, but not available until 3.7
    sub_cmds = parser.add_subparsers(dest='sub')

    build_cmd = sub_cmds.add_parser('build', help='Builds all firmware targets')

    test_cmd = sub_cmds.add_parser('test', help='Runs all firmware unit tests')

    metrics_cmd = sub_cmds.add_parser(
        'metrics', help='Write all firmware build metrics to file')

    metrics_cmd.add_argument(
        '--file',
        dest='file',
        required=True,
        # TODO(jettrink): update help message with final proto message
        help='File to write a json-encoded proto Message.',
    )

    return parser.parse_args(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

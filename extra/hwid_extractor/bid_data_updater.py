#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This Tool is for the extractions of HWID and serial no.

Quick start:
  sudo python hwid_extractor.py
"""

import argparse


def parse_arguments(raw_args):
    """Parse command line arguments"""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-v',
        '--verbosity',
        action='count',
        default=0,
        help='Logging verbosity.')
    args = parser.parse_args(raw_args)
    return args


def main(raw_args):
    """main function"""
    args = parse_arguments(raw_args)
    logging.basicConfig(level=logging.WARNING - args.verbosity * 10)
    # TODO(chungsheng@): Add implementation
    raise NotImplementedError('TODO')


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

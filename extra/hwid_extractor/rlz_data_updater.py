#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This Tool is for the extractions of HWID and serial no.

Quick start:
  sudo python hwid_extractor.py
"""


import argparse
import json
import os
import subprocess
import sys


ALL_DEVICE_JSON_GS = 'gs://chromeos-build-release-console/all_devices.json'
GSUTIL_BIN = '/usr/bin/gsutil'
ALL_DEVICE_JSON_LOCAL = os.path.join(os.path.dirname(__file__), 'all_device.json')
RLZ_JSON = os.path.join(os.path.dirname(__file__), 'rlz.json')


def parse_arguments(raw_args):
    """Parse command line arguments"""
    parser = argparse.ArgumentParser()
    args = parser.parse_args(raw_args)
    return args


def main(raw_args):
    """main function"""
    args = parse_arguments(raw_args)
    subprocess.run([GSUTIL_BIN, 'cp', ALL_DEVICE_JSON_GS, ALL_DEVICE_JSON_LOCAL], check=True)
    with open(ALL_DEVICE_JSON_LOCAL, 'r') as f:
        all_device = json.load(f)

    rlz = {}
    for device in all_device['devices']:
        cr50_board_id = device.get('cr50_board_id')
        if not cr50_board_id:
            continue
        reference_board = device.get('reference_board')
        if not reference_board:
            continue
        rlz[cr50_board_id] = reference_board['public_codename']

    with open(RLZ_JSON, 'w') as f:
        json.dump(rlz, f)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
    Helper utility for uploading software artifacts to memfault
"""

import requests
import os
import sys
import re

ORG = 'chromeos'
PROJECT = 'zork'

def upload_artifact(user, user_pass, organization, project, software_type, software_version, artifact):
    url = f'https://files.memfault.com/api/v0/organizations/{organization}/projects/{project}/software_types/{software_type}/software_versions/{software_version}/artifacts/symbols'
    headers = {
      'Content-Type': 'application/octet-stream'
    }
    auth = requests.auth.HTTPBasicAuth(user, user_pass)
    print(f'Uploading artifact of {len(artifact)} bytes to {url}')
    response = requests.request("PUT", url, auth=auth, headers=headers, data=artifact)
    print('Put Response:', response.text)

def delete_artifact(user, user_pass, organization, project, software_type, software_version):
    url = f'https://files.memfault.com/api/v0/organizations/{organization}/projects/{project}/software_types/{software_type}/software_versions/{software_version}/artifacts/symbols'
    headers = {
      'Content-Type': 'application/octet-stream'
    }
    auth = requests.auth.HTTPBasicAuth(user, user_pass)
    print(f'Deleting artifact at {url}')
    response = requests.request("DELETE", url, auth=auth, headers=headers)
    print('Delete Response:', response.text)

def print_usage():
    print('Usage:')
    print(f'\t{__file__} <build dir>')
    print('\t** MEMFAULT_PASSWORD and MEMFAULT_EMAIL environ variables must be set **')

def main(argv: list):
    if len(argv) != 1:
        print("Error: Invalid arguments")
        print_usage()
        return -1

    user_api_key = os.environ.get('MEMFAULT_PASSWORD')
    if not user_api_key:
        print('Error: MEMFAULT_PASSWORD is not set')
        print_usage()
        return -1
    username = os.environ.get('MEMFAULT_EMAIL')
    if not username:
        print('Error: MEMFAULT_EMAIL is not set')
        print_usage()
        return -1

    build_dir = argv[0]
    version = None
    timestamp = None
    for line in open(os.path.join(build_dir, 'ec_version.h')):
        result = re.match(r'#define VERSION \"(.*)\"$', line)
        if result:
            version = result[1]
            print(f"Found version {version}")
        result = re.match(r'#define TIMESTAMP (.*)$', line)
        if result:
            timestamp = result[1]
            print(f"Found timestamp {timestamp}")
    if not version or not timestamp:
        print("Error: No version info found")
        return -1

    ro_elf = open(os.path.join(build_dir, 'RO/ec.RO.elf'), 'rb').read()
    rw_elf = open(os.path.join(build_dir, 'RW/ec.RW.elf'), 'rb').read()

    board = version.split('_')[0]
    version = f'{version}:{timestamp}'

    # delete_artifact(username, user_api_key, ORG, PROJECT, f'{board}_EC_RO', version)
    upload_artifact(username, user_api_key, ORG, PROJECT, f'{board}_EC_RO', version, ro_elf)

    # delete_artifact(username, user_api_key, ORG, PROJECT, f'{board}_EC_RW', version)
    upload_artifact(username, user_api_key, ORG, PROJECT, f'{board}_EC_RW', version, rw_elf)

if __name__ == '__main__':
   sys.exit(main(sys.argv[1:]))



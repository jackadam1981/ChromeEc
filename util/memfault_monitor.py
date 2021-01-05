#!/usr/bin/env python3

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
    Monitors memfault for chunks and uploads to memfault service
"""

import requests
import os
import sys
import time

device_serial = '1234567'
project_api_key = 'lXdlecixTUwE0S9DoZ9gGYzO8XXF9i6H'

chunks_url = f'https://chunks.memfault.com/api/v0/chunks/{device_serial}'

headers = {
  'Memfault-Project-Key': project_api_key,
  'Content-Type': 'application/octet-stream'
}

def upload_chunk(chunk):
    print('Uploading chunk of size', len(chunk), 'to', chunks_url)
    response = requests.request("POST", chunks_url, headers = headers, data = chunk)
    print('RESPONSE', response.text)

def main(argv: list):
    while True:
        chunk = os.popen('ectool memfault').read().strip()
        if chunk:
            print('Got hex chunk of size',len(chunk))
            print(chunk)
            chunk = bytes.fromhex(chunk)
            print('Got binary chunk of size',len(chunk))
            upload_chunk(chunk)
            continue
        time.sleep(5)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

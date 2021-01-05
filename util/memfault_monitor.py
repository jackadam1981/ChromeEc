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
import uuid
import syslog

project_api_key = 'lXdlecixTUwE0S9DoZ9gGYzO8XXF9i6H'
device_serial = format(uuid.getnode(), 'x') # MAC Address

chunks_url = f'https://chunks.memfault.com/api/v0/chunks/{device_serial}'
headers = {
  'Memfault-Project-Key': project_api_key,
  'Content-Type': 'application/octet-stream'
}

def upload_chunks(chunks):
    for chunk in chunks:
        syslog.syslog('Uploading chunk of size %d to %s' % (len(chunk), chunks_url))
        response = requests.request("POST", chunks_url, headers = headers, data = chunk)
        syslog.syslog('RESPONSE: %s' % (response.text))

def main(argv: list):
    while True:
        chunks = []
        while True:
            chunk = os.popen('ectool memfault').read().strip()
            if not chunk:
                break
            chunks.append(bytes.fromhex(chunk))            
        upload_chunks(chunks)
        time.sleep(5)

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

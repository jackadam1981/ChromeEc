#!/usr/bin/env python3

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
    Monitors memfault via ec console for chunks and uploads to memfault service
"""

import requests
import os
import sys
import time
import syslog
import re
import logging

POLLING_INTERVAL = 10

project_api_key = 'lXdlecixTUwE0S9DoZ9gGYzO8XXF9i6H'

chunks_url = 'https://chunks.memfault.com/api/v0/chunks/'
headers = {
  'Memfault-Project-Key': project_api_key,
  'Content-Type': 'application/octet-stream'
}
memfault_app_device_url = 'https://app.memfault.com/organizations/chromeos/projects/zork/devices'

def upload_chunks(device_id, chunks):
    chunks_device_url = chunks_url + device_id
    for chunk in chunks:
        print('Uploading chunk of size %d to %s' % (len(chunk), chunks_device_url))
        response = requests.request("POST", chunks_device_url, headers = headers, data = chunk)
        print('Memfault response: %s' % (response.text))

def get_chunk():
  memfault_chunk_regex = '([0-9]+):([0-9a-f]*)\\r\\n'

  if os.popen(f'dut-control ec_uart_regexp:\'["{memfault_chunk_regex}"]\'').close():
    raise Exception('Error setting ec_uart_regexp')

  if os.popen(f'dut-control ec_uart_cmd:memfault_chunk').close():
    raise Exception('Error setting ec_uart_cmd')

  response = os.popen(f'dut-control -o ec_uart_cmd').read()
  if not response:
    raise Exception('Error getting ec_uart_cmd')

  response = eval(response)

  if type(response) != list or \
     len(response) != 1 or \
     type(response[0]) != tuple or \
     len(response[0]) != 3:
    raise Exception(f'ec_uart_cmd response is invalid: {response}')

  chunk_len = int(response[0][1])
  chunk = bytes.fromhex(response[0][2])
  if chunk_len != len(chunk):
    raise Exception("Chunk length not valid, expected: %d got: %d" % (chunk_len, len(chunk)))

  if chunk_len != 0:
    print('Got chunk of length', chunk_len)

  return chunk

def main(argv: list):
  if (len(argv) != 1):
    print("Invalid arguments, must specify device id")
    return -1

  device_id = argv[0]
  print(f'''
{'*'*120}
*
* Memfault console monitor daemon started for device {device_id}
*
* See results at {memfault_app_device_url}/{device_id}
*
{'*'*120}
''')

  while True:
    chunks = []
    while True:
        try:
          chunk = get_chunk()
        except Exception as e:
          print(e)
          break
        if not chunk:
            break
        chunks.append(chunk)
    upload_chunks(device_id, chunks)
    time.sleep(POLLING_INTERVAL)

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

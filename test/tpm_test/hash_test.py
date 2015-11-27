#!/usr/bin/python
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for testing hash functions using extended commands."""

from __future__ import print_function

import hashlib
import struct

import utils

HASH = 1

# Hash command modes
CMD_START  = 0
CMD_CONT   = 1
CMD_FINISH = 2
CMD_SINGLE = 3

# Hash modes
MODE_SHA1 = 0
MODE_SHA256 = 1


class HashError(Exception):
  pass

EMPTY_RESPONSE = ''.join('%c' % int('%s' % x, 16) for
                         x in '80 01 00 00 00 0c ba cc d0 0a 00 01'.split())
test_inputs = (
  # name                        text
  ( 'sha1:single 0', 'anything really will work here' ),
  ( 'sha256:single 0', 'some more text, this time for sha256' ),
  ( 'sha256:start 1', 'some more text, this time for sha256' ),
  ( 'sha256:cont 1', 'some more text, this time for sha256' ),
  ( 'sha256:start 2', 'this could be anything, we just need to' ),
  ( 'sha1:start 3', 'let\'s interleave a sha1 calculation' ),
  ( 'sha256:cont 2', 'fill up a second context with something' ),
  ( 'sha256:cont 1', 'let\'s feed some more into context 1' ),
  ( 'sha256:finish 1', 'some more text, this time for sha256' ),
  ( 'sha1:cont 3', 'with two active sha256 calculations' ),
  ( 'sha1:finish 3', 'this should be enough' ),
  ( 'sha256:finish 2', 'it does not really matter what' ),
)
def hash_test(tpm):
  """Exercise multiple hash threads simultaneously.

    Command structure, shared out of band with the test running on the target:

    field     |    size  |                  note
    ===================================================================
    hash_cmd  |    1     | 0 - start, 1 - cont., 2 - finish, 4 - single
    hash_mode |    1     | 0 - sha1, 1 - sha256
    handle    |    1     | session handle, ignored in 'single' mode
    text_len  |    2     | size of the text to process, big endian
    text      | text_len | text to hash
  """

  contexts = {}

  mode_map = {
    'sha1': MODE_SHA1,
    'sha256': MODE_SHA256
  }

  cmd_map = {
    'start': CMD_START,
    'cont': CMD_CONT,
    'finish': CMD_FINISH,
    'single': CMD_SINGLE
  }

  for test in test_inputs:
    name, text = test

    title, handle = name.split()
    mode_name, cmd_name = title.split(':')
    handle = int(handle)
    hash_mode = mode_map[mode_name]
    hash_cmd = cmd_map[cmd_name]

    cmd = '%c' % hash_cmd
    cmd += '%c' % hash_mode
    cmd += '%c' % handle   # Ignored for singe shots
    cmd += struct.pack('>H', len(text))
    cmd += text
    wrapped_response = tpm.command(tpm.wrap_ext_command(HASH, cmd))
    if hash_mode == MODE_SHA1:
      hfunc = hashlib.sha1
    elif hash_mode == MODE_SHA256:
      hfunc = hashlib.sha256
    else:
      raise HashError('unsupported hash mode %d' % hash_mode)
    if hash_cmd in (CMD_START, CMD_CONT):
      if hash_cmd == CMD_START:
        contexts[handle] = hfunc()
      h = contexts[handle]
      h.update(text)
      if wrapped_response != EMPTY_RESPONSE:
        raise HashError("Unexpected response to '%s': %s" %
                        (title, utils.hex_dump(wrapped_response)))
      continue
    if hash_cmd == CMD_FINISH:
      h = contexts[handle]
    elif hash_cmd == CMD_SINGLE:
      h = hfunc()
    else:
      raise HashError('Unknown command %d' % hash_cmd)
    h.update(text)
    digest = h.digest()
    result = wrapped_response[12:]
    if result != h.digest():
      raise HashError('%s error:%s%s' % (name,
                                         utils.hex_dump(digest),
                                         utils.hex_dump(result)))
    print('%sSUCCESS: %s' % (utils.cursor_back(), name))

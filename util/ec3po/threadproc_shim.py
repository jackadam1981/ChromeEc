# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a shim library for the ec3po transition from subprocesses to threads.

This is necessary because ec3po is split between the platform/ec/ and
third_party/hdctools/ repositories, so the transition cannot happen atomically
in one change.  See http://b/79684405 #39.
"""

# Imports for use internally in this module.
import ctypes as _ctypes
import multiprocessing as _multiprocessing
import os as _os
import socket as _socket


# Imports to bring objects into this namespace for users of this module.
from Queue import Queue
from threading import Thread as ThreadOrProcess


def Pipe(duplex=True):
  if duplex:
    return _socket.socketpair()
  rd_fd, wr_fd = _os.pipe()
  return _os.fdopen(rd_fd, 'r'), _os.fdopen(wr_fd, 'w')


def Value(typecode_or_type, *args):
  ctype = _multiprocessing.typecode_to_type.get(
      typecode_or_type, typecode_or_type)
  return ctype(*args)

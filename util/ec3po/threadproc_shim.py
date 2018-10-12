# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a shim library for the ec3po transition from subprocesses to threads.

This is necessary because ec3po is split between the platform/ec/ and
third_party/hdctools/ repositories, so the transition cannot happen atomically
in one change.  See http://b/79684405 #39.

This contains only the multiprocessing objects or threading-oriented equivalents
that are actually in use by ec3po.  There is no need for further functionality,
because this shim will be deleted after the migration is complete.

TODO(b/79684405): After both platform/ec/ and third_party/hdctools/ sides of
ec3po have been updated to use this library, replace the multiprocessing
implementations with threading-oriented equivalents.

TODO(b/79684405): After this library has been updated to contain
threading-oriented equivalents to its original multiprocessing implementations,
and some reasonable amount of time has elapsed for thread-based ec3po problems
to be discovered, migrate both the platform/ec/ and third_party/hdctools/ sides
of ec3po off of this shim and then delete this file.
"""

# Imports for use internally in this module.
import Queue as _queue
import ctypes as _ctypes
import multiprocessing as _multiprocessing
import threading as _threading

# Imports to bring objects into this namespace for users of this module.
from Queue import Queue
from threading import Thread as ThreadOrProcess

_EOF_SENTINEL = object()


class _Connection(object):

  def __init__(self, read_queue, read_eof_event, read_eof_lock, write_queue,
               write_eof_event, write_eof_lock):
    self._closed = False
    self._lock = _threading.Lock()

    self._read_queue = read_queue
    self._read_eof_event = read_eof_event
    self._read_eof_lock = read_eof_lock

    self._write_queue = write_queue
    self._write_eof_event = write_eof_event
    self._write_eof_lock = write_eof_lock

  def send(self, obj):
    with self._lock:
      if self._write_queue is None:
        raise IOError(errno.EBADF, 'not open for writing')
      if self._closed:
        raise IOError(errno.EBADF, 'this end of the pipe is closed')
      with self._write_eof_lock:
        if self._write_eof_event.is_set():
          raise IOError(errno.EPIPE, 'other end of pipe is closed')
        self._write_queue.put(obj)

  def recv(self):
    with self._lock:
      if self._read_queue is None:
        raise IOError(errno.EBADF, 'not open for reading')
      if self._closed:
        raise IOError(errno.EBADF, 'this end of the pipe is closed')
      if self._read_queue is _EOF_SENTINEL:
        raise EOFError('other end of pipe is closed')
      obj = self._read_queue.get()
      if obj is _EOF_SENTINEL:
        self._read_queue = _EOF_SENTINEL
        raise EOFError('other end of pipe is closed')
      return obj

  def close(self):
    with self._lock:
      if self._closed:
        return
      if self._write_queue is not None:
        self._write_queue.put(_EOF_SENTINEL)
      if self._read_queue is not None:
        with self._read_eof_lock:
          self._read_eof_event.set()
      self._closed = True


def Pipe(duplex=True):
  c0_to_c1_queue = _queue.Queue() if duplex else None
  c0_to_c1_eof_event = _threading.Event() if duplex else None
  c0_to_c1_eof_lock = _threading.Lock() if duplex else None
  c1_to_c0_queue = _queue.Queue()
  c1_to_c0_eof_event = _threading.Event()
  c1_to_c0_eof_lock = _threading.Lock()
  return (
      _Connection(c1_to_c0_queue, c1_to_c0_eof_event, c1_to_c0_eof_lock,
                  c0_to_c1_queue, c0_to_c1_eof_event, c0_to_c1_eof_lock),
      _Connection(c0_to_c1_queue, c0_to_c1_eof_event, c0_to_c1_eof_lock,
                  c1_to_c0_queue, c1_to_c0_eof_event, c1_to_c0_eof_lock))


def Value(ctype, *args):
  return ctype(*args)

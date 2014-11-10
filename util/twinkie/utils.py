# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility classes for the other modules of the test framework."""

from __future__ import print_function

class Log(object):
  """Abstract the logging interface."""
  ERROR = 0
  WARN = 1
  INFO = 2
  DEBUG = 3

  def __init__(self):
    self._level = self.DEBUG

  def logPrint(self, level, text):
    if self._level >= level:
      print(text)

  def Debug(self, text):
    self.logPrint(self.DEBUG, text)

  def Info(self, text):
    self.logPrint(self.INFO, text)

  def Warn(self, text):
    self.logPrint(self.WARN, text)

  def Error(self, text):
    self.logPrint(self.ERROR, text)

#!/usr/bin/env python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""USB Power Delivery packet builder."""

from __future__ import print_function

from utils import Log

class PDPacket(object):
  """Interface to a Twinkie dongle through a USB connection."""
  def __init__(self, log=None):
    self.log = log if log else Log()

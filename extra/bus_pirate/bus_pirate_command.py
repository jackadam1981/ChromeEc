#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes


class PeripheralCmd(ctypes.Structure):
    _fields_ = [
        ('cs', ctypes.c_uint8, 1),
        ('aux', ctypes.c_uint8, 1),
        ('pullups', ctypes.c_uint8, 1),
        ('power', ctypes.c_uint8, 1),
        ('cmd', ctypes.c_uint8, 4),
    ]

    def __init__(self, cmd):
        super(PeripheralCmd, self).__init__()
        self.cs = 0
        self.aux = 0
        self.pullups = 0
        self.power = 0
        self.cmd = cmd


class WriteCmd(ctypes.Structure):
    _fields_ = [
        ('num_bytes', ctypes.c_uint8, 4),
        ('cmd', ctypes.c_uint8, 4),
    ]

    def __init__(self, num_bytes, cmd):
        if num_bytes < 1 or num_bytes > 16:
            raise ValueError('Can send 1 to 16 bytes at once')
        super(WriteCmd, self).__init__()
        self.num_bytes = num_bytes - 1
        self.cmd = cmd

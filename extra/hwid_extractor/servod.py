#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
"""

import subprocess
import logging
# import sys
import time

from chromite.lib.firmware.flash_ap import DutControl

SERVOD_BIN = '/usr/bin/servod'
SERVOD_KILL_TIMEOUT = 3


class Servod():
    """
    """

    def __init__(self, port=9999, board=None, serial_name=None):
        self._port = port
        self._board = board
        self._serial_name = serial_name

    def _check_servod_alive(self):
        if self._servod.returncode != None:
            raise RuntimeError('Servod unexpectedly stopped.')

    def __enter__(self):
        servod_cmd = [SERVOD_BIN, '-p', str(self._port)]
        if self._board:
            servod_cmd += ['-b', self._board]
        if self._serial_name:
            servod_cmd += ['-s', self._serial_name]

        self._servod = subprocess.Popen(
            servod_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1)
        self._check_servod_alive()
        return DutControl(self._port)

    def __exit__(self, *args, **kargs):
        if self._servod.returncode != None:
            return
        self._servod.terminate()
        try:
            self._servod.wait(SERVOD_KILL_TIMEOUT)
        except subprocess.TimeoutExpired:
            self._servod.kill()

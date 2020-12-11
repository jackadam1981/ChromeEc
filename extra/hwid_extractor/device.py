#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import os
import logging
import subprocess
import re

from hwid_extractor.servod import Servod
from hwid_extractor.cr50 import Cr50
from hwid_extractor import ap_firmware

BID_JSON = os.path.join(os.path.dirname(__file__), 'bid.json')
if not os.path.isfile(BID_JSON):
    raise Error('Cannot find bid.json. Try: make bid.json')
with open(BID_JSON, 'r') as f:
    BID_DATA = json.load(f)

CR50_USB = '18d1:5014'
CR50_LSUSB_CMD = ['lsusb', '-vd', CR50_USB]
CR50_LSUSB_SERIAL_RE = r'iSerial +\d+ (\S+)\s'


def _scan_cr50_devices():
    """Use `lsusb` to get iSerial attribute of Cr50 devices

    Return:
      First serial name of Cr50 devices in uppercase.
    """
    logging.info('Scan serial names of Cr50 devices')
    try:
        output = subprocess.check_output(CR50_LSUSB_CMD, encoding='utf-8')
    except subprocess.CalledProcessError:
        # No Cr50 device is found.
        return None
    serials = re.findall(CR50_LSUSB_SERIAL_RE, output)
    if len(serials) == 0:
        # Since `lsusb` success, iSerial should be listed in the output of
        # `lsusb`. If not, the user may not have permision to get iSerial.
        raise RuntimeError(
            'Cannot get Cr50 serial number. Maybe running with sudo ?')
    if len(serials) > 1:
        logging.warning('Working with multiple devices is not supported.')
    logging.info(f'Serial numbers of Cr50 devices: {serials}')
    return serials[0].upper()


@contextlib.contextmanager
def _get_cr50_from_servod(*args, **kargs):
    with Servod(*args, **kargs) as dut_control:
        dut_control.run(['cr50_uart_timestamp:off'])
        cr50_pty = dut_control.get_value('cr50_uart_pty')
        yield Cr50(cr50_pty)


def scan():
    cr50_serial_name = _scan_cr50_devices()
    with _get_cr50_from_servod(serial_name=cr50_serial_name) as cr50:
        bid = cr50.get_bid()
        data = {
            'cr50SerialName': cr50_serial_name,
            'bid': bid,
            'referenceBoard': BID_DATA.get(bid),
            'challenge': cr50.get_challenge(),
            'isRestricted': cr50.is_restricted(),
            'isTestlabEnabled': cr50.is_testlab_enabled(),
        }
    return data


def extract_hwid_and_serial_number(cr50_serial_name, board):
    with Servod(serial_name=cr50_serial_name, board=board) as dut_control:
        return ap_firmware.extract_hwid_and_serial_number(board, dut_control)


def unlock(cr50_serial_name, authcode):
    with _get_cr50_from_servod(serial_name=cr50_serial) as cr50:
        return cr50.unlock(authcode)


def lock(cr50_serial_name):
    with _get_cr50_from_servod(serial_name=cr50_serial) as cr50:
        return cr50.lock()


def enable_testlab(cr50_serial_name):
    pass

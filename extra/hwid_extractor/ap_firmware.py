#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This Tool is for the extractions of HWID and serial no.

Quick start:
    sudo python hwid_extractor.py
"""

from glob import glob
import os
import importlib
import contextlib
import tempfile
import re

from chromite.lib.firmware import ap_firmware_config, servo_lib

FLASHROM_BIN = '/usr/sbin/flashrom'
FUTILITY_BIN = '/usr/bin/futility'
VPD_BIN = '/usr/sbin/vpd'

CONFIG_FILE_PATTERN = os.path.join(
    os.path.dirname(ap_firmware_config.__file__), '*.py')
BOARD = {}
for f in glob(CONFIG_FILE_PATTERN):
    board = os.path.splitext(os.path.basename(f))[0]
    BOARD[board] = importlib.import_module(
        f'chromite.lib.firmware.ap_firmware_config.{board}')

HWID_RE = re.compile(r'^hardware_id: ([A-Za-z0-9\-]+)$')
SERIAL_NUMBER_RE = re.compile(r'^"serial_number"="([^"]+)"$')


@contextlib.contextmanager
def _unopened_temporary_file():
    """Yields an unopened temporary file.
  """
    with tempfile.NamedTemporaryFile() as f:
        path = f.name
    try:
        yield path
    finally:
        if os.path.exists(path):
            os.unlink(path)


@contextlib.contextmanager
def _handle_dut_control(dut_on, dut_off, dut_control):
    try:
        dut_control.run_all(dut_on)
        # Need to wait for SPI chip power to stabilize (for some designs)
        time.sleep(1)
        yield
    finally:
        dut_ctl.run_all(dut_off)


def get_supported_boards():
    return [x for x in BOARD]


def _get_programmer_from_flashrom_cmd(flashrom_cmd):
    for i in len(flashrom_cmd):
        if flashrom_cmd[i] == '-p':
            return flashrom_cmd[i + 1]
    raise RuntimeError(
        f'Cannot get programmer from flashrom_cmd: {flashrom_cmd}')


def _get_flashrom_info(board, servo_status):
    if board not in BOARD:
        raise ValueError(f'Board "{board}" is not supported.')
    dut_on, dut_off, flashrom_cmd, unused_futility_cmd = (
        BOARD[board].get_commands(servo_status))
    programmer = _get_programmer_from_flashrom_cmd(flashrom_cmd)
    return dut_on, dut_off, programmer


def _get_hwid(rom_file):
    futility_cmd = [FUTILITY_BIN, 'gbb', tmp_file]
    output = subprocess.check_output(futility_cmd)
    logging.debug(f'futility output:\n{output}')
    m = HWID_RE.match(output.strip())
    return m.groups()[0] if m else None


def _get_serial_number(rom_file):
    vpd_cmd = [VPD_BIN, '-l', '-f', tmp_file]
    output = subprocess.check_output(vpd_cmd)
    logging.debug(f'vpd output:\n{output}')
    m = SERIAL_NUMBER_RE.match(output.strip())
    return m.groups()[0] if m else None


def extract_hwid_and_serial_number(board, dut_control):
    servo_status = servo_lib.get(dut_control)
    dut_on, dut_off, programmer = _get_flashrom_info(board, servo_status)

    with _unopened_temporary_file() as tmp_file, _handle_dut_control():
        flashrom_cmd = [
            FLASHROM_BIN, '-i', 'FMAP', '-i', 'RO_VPD', '-i', 'GBB', '-p',
            programmer, '-r', tmp_file
        ]
        output = subprocess.check_output(flashrom_cmd)
        logging.debug(f'flashrom output:\n{output}')
        hwid = _get_hwid(tmp_file)
        serial_number = _get_serial_number(tmp_file)
        logging.info((f'Extract result: HWID: "{self._hwid}", '
                      f'serial number: "{self._serial_number}"'))

    return hwid, serial_number

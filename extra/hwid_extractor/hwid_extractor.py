#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This Tool is for the extractions of HWID and serial no.

Quick start:
  sudo python hwid_extractor.py
"""

import argparse
import logging
import sys
import time

import serial

# Timeout for the unresponding serial console.
SERIAL_CONSOLE_TIMEOUT = 0.3
# Communicate with a console which may not be a Cr50 console may block. Add
# limits to prevent blocking. When the limit reach the command is considered
# to be failed.
CR50_CONSOLE_BUFFER_SIZE_BYTES = 4096
CR50_CONSOLE_MAX_DROPPING_LINE = 32
# Console may not ready to communicate. Retry if the output of the command is
# empty. After these retries the command is considered to be failed.
CR50_GENERAL_MAX_RETRY = 3
CR50_GENERAL_RETRY_INTERVAL = 1
# `rma_auth` may fail if the interval between the last call is too short. Retry
# if the output of the command is empty. After these retries the command is
# considered to be failed.
CR50_RMA_AUTH_MAX_RETRY = 3
CR50_RMA_AUTH_RETRY_INTERVAL = 1


class Device():
    """An interface object for communicating with devices through uart consoles.

    Attributes:
        cr50_serial_id: The serial id of the cr50 console.

    Args:
        cr50_console: The device of cr50 console.
        ec_console: The device of ec console. If None, the ec console is
            determined by the name of cr50 console.
    """

    def __init__(self, cr50_console, ec_console=None):
        self._cr50_console = cr50_console
        self._ec_console = ec_console if ec_console else self._get_ec_console()
        self._cr50_serial_id = self._get_cr50_serial_id()
        self._is_restricted = None
        self._challenge = None
        self._hwid = None
        self._serial_number = None

        if not self._cr50_serial_id:
            return
        self.update_restricted_status()

    @property
    def cr50_serial_id(self):
        """cr50_serial_id"""
        return self._cr50_serial_id

    def _get_ec_console(self):
        """Get the ec console from the name of cr50 console"""
        prefix = '/dev/ttyUSB'
        if not self._cr50_console.startswith(prefix):
            raise ValueError('cr50 console should match /dev/ttyUSB*')
        console_id = int(self._cr50_console[len(prefix):])
        # The ec console is the next two console of cr50.
        return f'/dev/ttyUSB{console_id + 2}'

    @staticmethod
    def _drop_unused_cr50_console_output(ser):
        """Drop the unused output from cr50 console.

        Sometime console will print some debug information and makes the parsing
        difficult. Drop them from the output. If the number of lines exceed
        `CR50_CONSOLE_MAX_DROPPING_LINE`, then we assume that this console has
        unlimited output and should not be a Cr50 serial. i.e. an AP console.

        Returns:
            True if all the output has been dropped.
        """
        for unused_i in range(CR50_CONSOLE_MAX_DROPPING_LINE):
            output = ser.read_until('\n', CR50_CONSOLE_BUFFER_SIZE_BYTES)
            # If the output is empty, we assume that all the outputs of cr50
            # console are dropped.
            if not output:
                return True
        return False

    @classmethod
    def _cr50_command_inner(cls, ser, cmd):
        """Execute the command on the cr50 console.

        Returns:
            The output of the command, or None.
        """
        if not cls._drop_unused_cr50_console_output(ser):
            return ''
        ser.write(f'{cmd}\n\n'.encode())
        cmd_token = f'{cmd}\r\n'.encode()
        output = ser.read_until(cmd_token, CR50_CONSOLE_BUFFER_SIZE_BYTES)
        # Cr50 console should print the command.
        if cmd_token not in output:
            return None
        # Cr50 console should print `>` at the last line.
        output = ser.read_until('>'.encode(), CR50_CONSOLE_BUFFER_SIZE_BYTES)
        try:
            return output.decode().rpartition('>')[0].strip()
        except UnicodeDecodeError:
            return None

    def _cr50_command(self,
                      cmd,
                      max_retry=CR50_GENERAL_MAX_RETRY,
                      retry_interval=CR50_GENERAL_RETRY_INTERVAL):
        """Execute the command on the cr50 console.

        It is not guarantee that the console is cr50 console, or the console is
        ready to be used. If the output of the command is empty, do some retry
        and give up after `max_retry` times.

        Returns:
            The output of the command.
        """
        logging.info('Execute command: "%s" on console: %s', cmd,
                     self._cr50_console)
        with serial.Serial(
                self._cr50_console, timeout=SERIAL_CONSOLE_TIMEOUT) as ser:
            logging.debug('Serial init.')
            for unused_i in range(max_retry):
                output = self._cr50_command_inner(ser, cmd)
                if output:
                    break
                time.sleep(retry_interval)
            if not output:
                logging.info(
                    'Output of Command: "%s" on console "%s" is empty.', cmd,
                    self._cr50_console)
            logging.debug('Cr50 command: "%s":\n%s', cmd, output)
        return output

    def _get_cr50_serial_id(self):
        """Find the serial id of the Cr50 console.

        DEV_ID of `sysinfo` command match `lsusb` iSerial. Try to execute
        `sysinfo` and return the serial id so we can use it to identify Cr50
        console.

        Returns:
            The serial id. i.e. 10005051-949B5348 in uppercase, or None.
        """
        logging.info('Try to get sysinfo from console: %s', self._cr50_console)
        sysinfo = self._cr50_command('sysinfo')
        for line in sysinfo.splitlines():
            tokens = line.split()
            if len(tokens) == 3 and tokens[0] == 'DEV_ID:':
                serial_id = f'{tokens[1][2:]}-{tokens[2][2:]}'.upper()
                logging.info('Cr50 serial: %s', serial_id)
                return serial_id
        logging.info('Cr50 serial not found.')
        return None

    def update_restricted_status(self):
        """Update the restricted status of the device.

        Check the output of `ccd` command. If it contain 'IfOpened' or
        'IfUnlocked', the device is considered as restricted. If 'Capabilities'
        is not in the output of `ccd` command, the execution of `ccd` command is
        considered as failed.

        Raises:
            ValueError: Cannot get the output of `ccd` from device.
        """
        logging.info('Update restricted status')
        output = self._cr50_command('ccd')
        if 'Capabilities' not in output:
            logging.error('`Capabilities` not in output of `ccd`, output:\n%s',
                          output)
            raise ValueError('Could not get ccd output.')
        self._is_restricted = 'IfOpened' in output or 'IfUnlocked' in output
        logging.info('Restricted status: %s', self._is_restricted)


def parse_arguments(raw_args):
    """Parse command line arguments"""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-v',
        '--verbosity',
        action='count',
        default=0,
        help='Logging verbosity.')
    args = parser.parse_args(raw_args)
    return args


def main(raw_args):
    """main function"""
    args = parse_arguments(raw_args)
    logging.basicConfig(level=logging.WARNING - args.verbosity * 10)
    # TODO(chungsheng@): Add implementation
    raise NotImplementedError('TODO')


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

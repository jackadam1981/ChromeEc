#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This Tool is for the extractions of HWID and serial no.

Quick start:
  sudo python hwid_extractor.py
"""

import logging
import time
import re
import codecs

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


class Cr50Console():

    def __init__(self, cr50_uart_pty):
        self._cr50_uart_pty = cr50_uart_pty

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

    def command(self,
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
                     self._cr50_uart_pty)
        with serial.Serial(
                self._cr50_uart_pty, timeout=SERIAL_CONSOLE_TIMEOUT) as ser:
            logging.debug('Serial init.')
            for unused_i in range(max_retry):
                output = self._cr50_command_inner(ser, cmd)
                if output:
                    break
                time.sleep(retry_interval)
            if not output:
                logging.info(
                    'Output of Command: "%s" on console "%s" is empty.', cmd,
                    self._cr50_uart_pty)
            logging.debug('Cr50 command: "%s":\n%s', cmd, output)
        return output


class Cr50():
    """An interface object for communicating with devices through uart consoles.

    Args:
        cr50_uart_pty: The device of cr50 console.
        ec_console: The device of ec console. If None, the ec console is
            determined by the name of cr50 console.
    """

    def __init__(self, cr50_uart_pty):
        self._cr50_console = Cr50Console(cr50_uart_pty)

    def get_bid(self):
        """
        Board ID: 57565257:a8a9ada8, flags 00007f7f
        """
        output = self._cr50_console.command('bid')
        tokens = output.split(':')
        print(f'tokens: {tokens}')
        if len(tokens) < 2:
            return None
        hex_bid = tokens[1].strip()
        print(f'hex_bid: {hex_bid}')
        return codecs.decode(hex_bid, 'hex').decode()

    def get_challenge(self):
        """Get the rma_auth challenge

        There are two challenge formats

        "
        ABEQ8 UGA4F AVEQP SHCKV
        DGGPR N8JHG V8PNC LCHR2
        T27VF PRGBS N3ZXF RCCT2
        UBMKP ACM7E WUZUA A4GTN
        "
        and
        "
        generated challenge:

        CBYRYBEMH2Y75TC...rest of challenge
        "
        support extracting the challenge from both.

        After `rma_auth` executed, the next execution may need to wait for about
        10s. Otherwise `rma_auth` may return 'RMA Auth error'. Do some retry if we
        get this error.

        Returns:
            The RMA challenge with all whitespace removed.
        """
        for i in range(CR50_RMA_AUTH_MAX_RETRY):
            output = self._cr50_console.command('rma_auth').strip()
            if 'RMA Auth error' not in output:
                break
            time.sleep(CR50_RMA_AUTH_RETRY_INTERVAL)
        if 'generated challenge:' in output:
            return output.split('generated challenge:')[-1].strip()
        challenge = ''.join(re.findall(r' \S{5}' * 4, output))
        # Remove all whitespace
        return re.sub(r'\s', '', challenge)

    def is_restricted(self):
        """Update the restricted status of the device.

        Check the output of `ccd` command. If it contain 'IfOpened' or
        'IfUnlocked', the device is considered as restricted. If 'Capabilities'
        is not in the output of `ccd` command, the execution of `ccd` command is
        considered as failed.

        Raises:
            ValueError: Cannot get the output of `ccd` from device.
        """
        logging.info('Update restricted status')
        output = self._cr50_console.command('ccd')
        if 'Capabilities' not in output:
            logging.error('`Capabilities` not in output of `ccd`, output:\n%s',
                          output)
            raise ValueError('Could not get ccd output.')
        is_restricted = 'IfOpened' in output or 'IfUnlocked' in output
        logging.info('Restricted status: %s', is_restricted)
        return is_restricted

    def is_testlab_enabled(self):
        pass

    def unlock(authcode):
        """Unlock the device with `authcode`

        If unlock success, Cr50 will reboot and may be unresponsive for several
        seconds.
        """
        logging.info(f'Unlock the device with authcode: {authcode}')
        output = self._cr50_console.command(f'rma_auth {authcode}')
        logging.info(f'Unlock result:\n{output}')
        return 'process_response: success!' in output

    def lock():
        """Lock the device

        After `ccd reset` and `ccd lock` the device may be unresponsive for several
        seconds.
        """
        logging.info('Lock the device')
        self._cr50_console.command(f'ccd open')
        self._cr50_console.command(f'ccd reset')
        time.sleep(1)
        self._cr50_console.command(f'ccd lock')
        time.sleep(1)
        self.UpdateRestrictedStatus()
        return self.is_restricted()

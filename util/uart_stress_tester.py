#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# ChromeOS Chargen Test
#
# This tester runs the command 'chargen' to each UART device, captures the
# output, and compares it against the expected output to check any characters
# lost.
#
# Prerequisite:
#     If servod is running, turn uart_timestamp off before running this test.
#     e.g. dut-control cr50_uart_timestamp:off
#
from __future__ import print_function
import argparse
import atexit
import io
import logging
import os
import re
import serial
import sys
import threading
import time

DURATION = 30
BAUDRATE = 115200
KERNEL_USERNAME = 'root'
KERNEL_PASSWORD = 'test0000'
CHARGEN_TXT = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
CR = '\r'
LF = '\n'
TPM_CLI = 'while true; do   trunks_client --key_create --rsa=2048' \
          ' --usage=sign --key_blob=/tmp/blob > /dev/null;   sleep 1; done &'


class ChargenTestError(Exception):
  """Exception for Uart Stress Test Error"""
  pass


class UartSerial(object):
  """Test Object for a single UART serial device"""
  TEST_PROFILE_DICT = (
      # Sample
      {
        'prompt':'Prompt text to match',
        'device_type':'Kernel or EC',
      },
      # Kernel
      {
        'prompt':'localhost login:',
        'device_type':'kernel',
      },
      # EC
      {
        'prompt':'> ',
        'device_type':'EC',
      },
     )

  def __init__(self, port, duration, timeout=1, parity=serial.PARITY_EVEN,
               baudrate=BAUDRATE, cr50_workload=False):
    """
    Args:
      port: UART device path. e.g. /dev/ttyUSB0
      duration: Time to test, in seconds
      timeout: Read timeout value.
      parity: Parity checking. e.g. serial.PARITY_NONE or serial.PARITY_EVEN.
      baudrate: Baud rate such as 9600 or 115200.
    """
    self.serial = serial.Serial()
    self.serial.port = port
    self.serial.timeout = timeout
    self.serial.parity = parity
    self.serial.baudrate = baudrate

    self.duration = duration
    self.cr50_workload = cr50_workload

    self.logger = logging.getLogger(type(self).__name__)

    self.cleanup_cli = []
    self.test_cli = []
    self.test_expired = False
    self.eol = CR+LF
    self.is_ap = False
    self.num_ch_cap = 0

    atexit.register(self.cleanup)

  def run_command(self, command_lines, delay=1):
    """
    Args:
      command_lines: list of commands to run.
      delay: delay after a command in second
    """
    for cli in command_lines:
      self.logger.debug(self.serial.port + ': run ' + cli)

      self.serial.write(cli + self.eol)
      self.serial.flush()
      if delay:
        time.sleep(delay)

  def cleanup(self):
    """Before termination, clean up the UART device."""
    port = self.serial.port
    self.logger.debug(port + ': Closing...')

    try:
      self.serial.open()

      # Restore display channel status (EC/CR50) or log out (kernel)
      # Note: logout should be run last. Reverse the sequence in list.
      self.cleanup_cli.reverse()
      self.run_command(self.cleanup_cli)
      self.serial.close()
    except:
      self.logger.error(port + ': Cannot open.')
    finally:
      self.logger.debug(port + ': Closed.')

  def get_output(self, stop_char=''):
    """Capture the UART output

    Returns: the text from UART output.
    """
    uart_output = ''
    if stop_char:
      while self.serial.inWaiting() == 0:
        pass

      while True:
        tmp_char = self.serial.read(1)
        if tmp_char == stop_char:
          break
        uart_output += tmp_char
    else:
      while self.serial.inWaiting() > 0:
        uart_output += self.serial.read(1)

    return uart_output.replace(CR, '')

  def prepare(self):
    """Prepare the test:
        Identify the type of UART device (EC or Kernel?), then
        decide what kind of commands to use to generate stress loads.

    Raises: ChargenTestError if UART source cannot not identified.
    """
    try:
      self.serial.open()

      # Prepare the device for test
      self.serial.flushInput()
      self.serial.flushOutput()

      tmp_txt = self.get_output()      # drain data

      # Give a couple of line feed, and capture the prompt text
      self.run_command([''])
      prompt_txt = self.get_output()

      # Detect the device source: EC or AP?
      # Detect if the device is kernel or EC console based on the captured.
      for dev_prof in self.TEST_PROFILE_DICT:
        if dev_prof['prompt'] in prompt_txt:
          break
      else:
        # No prompt patterns were found. UART seems not responding or in
        # an undesirable status.
        raise ChargenTestError(self.serial.port + ' is not responding: ' +
                               prompt_txt)

      self.logger.debug(self.serial.port + ': is ' + dev_prof['device_type'] +
                        ' UART.')

      # Either login to kernel or run some commands to prepare the device
      # for test
      if dev_prof['device_type'] == 'kernel':
        # Login to kernel
        self.run_command([KERNEL_USERNAME, KERNEL_PASSWORD])
        self.cleanup_cli += ['logout']

        # Convert End of Line from CRLF to LF. Otherwise it will be regarded as
        # two line feed.
        self.eol = LF
        self.is_ap = True
      else:
        # Hush EC UART messages by turning off display channels.
        self.run_command(['chan save', 'chan 0'])
        self.cleanup_cli += ['chan restore']

      # Determine CLI to test
      # Initialize test command line and the frequency.
      tmp_txt = self.get_output()      # drain data before capture

      # Check whether the command 'chargen' is available in the device.
      # 'chargen 1 4' is supposed to print '0000'
      self.run_command(['chargen 1 4'])
      tmp_txt = self.get_output()

      # If chargen command is available, then update the test command
      # inforation.
      if '0000' in tmp_txt:
        self.num_ch_exp = int(self.serial.baudrate * self.duration / 10)
        self.test_cli = ['chargen %d %d' % (len(CHARGEN_TXT), self.num_ch_exp)]
      else:
        raise ChargenTestError('chargen command is not available.')
    finally:
      self.serial.close()

  def stress_test_thread_expire(self):
    """Expire the test timer"""
    self.test_expired = True

  def stress_test_thread(self):
    """test thread"""
    try:
      self.serial.open()
      tmp_txt = self.get_output()      # drain data

      # Run TPM command in background to burden cr50.
      if self.is_ap and self.cr50_workload:
        self.run_command([TPM_CLI])
        tmp_txt = self.get_output()      # drain data
        bg_pid = re.search(r'\[\d+\]\s(\d+)', tmp_txt).groups()[0]   # Catch PID
        self.cleanup_cli += ['kill -9 ' + bg_pid]
        self.logger.debug(self.serial.port + ': run TPM job, PID ' + bg_pid +
                          ' in background.')

      # Run the command 'chargen', one time
      self.run_command(self.test_cli)
      captured = self.get_output(LF)    # Drain the output

      # Keep capturing the output until the test timer is expired.
      self.num_ch_cap = 0
      self.char_loss_occurrences = 0
      total_num_ch = self.num_ch_exp
      ch_exp = ''
      while not self.test_expired and self.num_ch_cap < total_num_ch:
        captured = self.get_output(LF)
        self.num_ch_cap += len(captured)

        for ch_cap in captured:
          if not ch_cap.isalnum():
            self.logger.error(self.serial.port + ': Broken char captured ' \
                              'Expected ' + ch_exp + ' but got ' + ch_cap)
            ch_exp=''
          elif bool(ch_exp) and ch_exp != ch_cap:
            self.logger.error(self.serial.port + ': Detected characer loss: ' \
                              'Expected ' + ch_exp + ' but got ' + ch_cap)
            self.char_loss_occurrences += 1
            # TODO: check whether it is continuous
            idx_ch_exp = CHARGEN_TXT.find(ch_exp)
            idx_ch_cap = CHARGEN_TXT.find(ch_cap)
            if idx_ch_cap > idx_ch_exp:
              total_num_ch -= (idx_ch_cap - idx_ch_exp)
            else:
              total_num_ch -= (len(CHARGEN_TXT) - idx_ch_cap + idx_ch_exp)

          if ch_cap == 'z':
            ch_exp = '0'
          elif ch_cap == 'Z':
            ch_exp = 'a'
          elif ch_cap == '9':
            ch_exp = 'A'
          else:
            ch_exp = chr(ord(ch_cap) + 1)
    finally:
      self.serial.close()

  def start_test(self):
    """Start the test thread"""
    self.test_expired = False
    self.test_thread = threading.Thread(target=self.stress_test_thread)

    self.logger.info(self.serial.port + ': Test starts.')
    self.test_thread.start()

        # After the test duration, set up the test terminal condition
    t = threading.Timer(self.duration, self.stress_test_thread_expire)
    t.start()

  def wait_test_done(self):
    """Wait until the test thread get done and join"""
    self.test_thread.join()

    self.logger.info(self.serial.port + ': Test is done.')

  def get_result(self):
    """Display the result

    Returns:
      Integer = the number of lost character
    Raises:
      ChargenTestError: if the capture is corrupted.
    """

    # If more characters than expected are captured, it means some messages
    # from other than chargen are mixed. Stop processing further.
    if self.num_ch_exp < self.num_ch_cap :
      raise ChargenTestError('UART output from ' + self.serial.port +
                                ' is corrupted.')

    # Get the count difference between the expected to the captured
    # as the number of lost character.
    char_lost = self.num_ch_exp - self.num_ch_cap
    self.logger.info('%s: %d char lost / %d (%.1f %%), in %d occurrences' % (
                        self.serial.port, char_lost, self.num_ch_exp,
                        char_lost * 100.0 / self.num_ch_exp,
                        self.char_loss_occurrences))
    return char_lost, self.num_ch_exp, self.char_loss_occurrences


class ChargenTest(object):
  """UART stress tester"""

  def __init__(self, ports, duration, cr50_workload=False):
    """Setup UART stress tester

    Args:
      ports: List of UART ports to test.
      duration: Time to keep running test in seconds.
    """
    self.ports = ports

    if duration <= 0:
        raise ChargenTestError('Input error: duration is not positive.')
    self.duration = duration

    self.cr50_workload=cr50_workload

    # Logging setup
    self.logger = logging.getLogger(type(self).__name__)

    # Create a serial object for each device
    self.serials = {}     # Serial objects
    for port in self.ports:
      self.serials[port] = UartSerial(port=port, duration=self.duration,
                                      cr50_workload=self.cr50_workload)

  def prepare(self):
    """Prepare the test for each UART port"""
    for port, ser in self.serials.items():
      self.logger.info(port + ': Preparing...' )
      ser.prepare()
      self.logger.info(port + ': Ready to test.')

  def print_result(self):
    """Display the test result for each UART port"""
    char_lost = 0
    for port, ser in self.serials.items():
      (tmp_lost, tmp_exp, tmp_occurrences) = ser.get_result()
      char_lost += tmp_lost

    # If any characters are lost, then test fails.
    msg = 'lost %d character(s) from the test.' % char_lost
    if char_lost > 0:
      self.logger.error('FAIL: ' + msg)
    else:
      self.logger.info('PASS: ' + msg)

  def run(self):
    """Run the stress test on UART port(s)"""

    # Detect UART source type, and decide which command to test.
    self.prepare()

    # Run the test on each UART port in thread.
    self.logger.info('Test starts.')
    for port, ser in self.serials.items():
      ser.start_test()

    # Wait all tests to finish.
    for port, ser in self.serials.items():
      ser.wait_test_done()

    # Print the result.
    self.print_result()
    self.logger.info('Test is done.')


def parse_args(cmdline):
  """Parse command line arguments.

  Args:
    comdline: list, comdline to be parsed
  Returns:
    tuple (options, args) where args is a list of cmdline arguments that the
    parser was unable to match i.e. they're servod controls, not options.
  """
  description = """%(prog)s repeats sending a uart console command
to each UART device for a given time, and check if output
has any missing characters.

Examples:
    %(prog)s /dev/ttyUSB2 --time 3600
    %(prog)s /dev/ttyUSB1 /dev/ttyUSB2 --debug
"""

  parser = argparse.ArgumentParser(description=description,
                                formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('port', type=str, nargs="*",
                      help='UART device path to test')
  parser.add_argument('-c', '--cr50', action='store_true', default=False,
                      help='generate TPM workload on cr50')
  parser.add_argument('-d', '--debug', action='store_true', default=False,
                      help='enable debug messages')
  parser.add_argument('-t', '--time', type=int,
                      help='Test duration in second', default=300)
  return parser.parse_known_args(cmdline)


def main():
  """Main function wrapper"""
  try:
    (options, args) = parse_args(sys.argv[1:])

    # Set Log format
    log_format = '%(asctime)s %(levelname)-6s | %(name)-16s'
    date_format = '%Y-%m-%d %H:%M:%S'
    if options.debug:
      log_format += ' | %(filename)s:%(lineno)4d:%(funcName)-18s'
      loglevel = logging.DEBUG
    else:
      loglevel = logging.INFO
    log_format += ' | %(message)s'

    logging.basicConfig(level=loglevel, format=log_format,
                        datefmt=date_format)

    # Create a ChargenTest object
    utest = ChargenTest(options.port, options.time, options.cr50)
    utest.run()    # Run

  except KeyboardInterrupt:
    sys.exit(0)

  except ChargenTestError as e:
    print('Error: ', str(e))
    sys.exit(1)

if __name__ == '__main__':
  main()

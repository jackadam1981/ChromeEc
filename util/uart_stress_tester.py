#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# ChromeOS UART Stress Test
#
# This tester repeats sending UART command to each UART device, compares the
# UART output on a console command against the expected output, and checks if
# there are any lost characters.
#
# Use: Run uart_stress_tester.py --help.
#
# Output: At the end of test, character loss rates and transfer rates are
#         displayed on screen.
#
# How it works:
#    1. Run a console command on each UART, and capture the output for a base
#       text in comparison.
#    2. Run a console command on a UART or more multiple times, and capture the
#       output.
#    3. Compare the captured output against the base text, and check if the base
#       pattern is repeated or if the character is lost.
#    4. Print the result
#
# Prerequisite:
#     Turn off CR50 and EC uart output channels with the console command
#     'chan 0'
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
import tempfile
import threading
import time

DURATION=30
BAUDRATE=115200
KERNEL_USERNAME='root'
KERNEL_PASSWORD='test0000'
CHARGEN_TXT='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'


class UartStressTestError(Exception):
  """ """
  pass


class UartSerial(serial.Serial):
  TEST_PROFILE_DICT=(
      # Sample
      {
        'prompt':'Prompt text to match',
        'device_type':'Kernel or EC',
        'default_cli':[ '1st UART Command to run for test',
                        '2nd UART Command to run for test', ],
      },
      # Kernel
      {
        'prompt':'localhost login:',
        'device_type':'kernel',
        'default_cli':[ '' ],
      },
      # EC
      {
        'prompt':'> ',
        'device_type':'EC',
        'default_cli':[ 'waitms 0', 'help' ],
      },
     )

  cleanup_cli=[]
  test_cli=[]
  test_cli_actual_count=0
  sample_txt=''
  test_expired=False
  use_chargen=False
  result_txt=''            # TODO(save to tempfile,rather than variable)

  def __init__(self, port, timeout=1, parity=serial.PARITY_EVEN,
               baudrate=BAUDRATE):
    """ """
    super(UartSerial, self).__init__()
    self.port=port
    self.timeout=1
    self.parity=parity
    self.baudrate=baudrate

    self.logger=logging.getLogger(type(self).__name__)

  def run_command(self, cli):
    """
    Args:
      cli =
    """
    self.write(cli + '\r\n')
    self.flush()

  def __del__(self):
    """ """
    self.open()
    for cli in self.cleanup_cli:
      self.run_command(cli)
    self.close()

  def get_output(self):
    """
    """
    # Capture the output
    time.sleep(1)        # Wait for the buffer to get ready.
    uart_output=''
    while self.inWaiting() > 0:
      uart_output+=self.read(1)
    return uart_output

  def test_configure(self, duration, use_chargen=False):
    """ """
    num_ch_exp=int(self.baudrate * duration / 10)
    try:
      self.open()

      # Prepare the device for test
      self.flushInput()
      self.flushOutput()

      # Give a line feed, and capture the output
      self.run_command('')
      prompt_txt=self.get_output()

      # Detect the device source: EC or AP?
      # Detect if the device is kernel or EC console based on the captured.
      for test_prof in self.TEST_PROFILE_DICT:
        if re.search(test_prof['prompt'], prompt_txt):
          break
      else:
        raise UartStressTestError("%s is not identified." % self.port)
      self.logger.debug("%s is %s uart." % (self.port, test_prof['device_type']))

      # Either login to kernel or run some commands to prepare the device
      # for test
      if test_prof['device_type'] == 'kernel':
        # Login to kernel
        if use_chargen:
          self.run_command(KERNEL_USERNAME)
          time.sleep(1)
          self.run_command(KERNEL_PASSWORD)
          time.sleep(1)

          self.cleanup_cli.append('logout')
      else:
        # Hush EC UART messages.
        self.run_command('chan save')
        self.run_command('chan 0')

        self.cleanup_cli.append('chan restore')

      # Determine CLI to test
      # Initialize test command line and the frequency.
      if use_chargen:
        # Check whether the command 'chargen' is available in the device.
        self.run_command('chargen {0} {0}'.format(len(CHARGEN_TXT)))
        sample_txt=self.get_output()

        # if chargen command is available, then update the test command
        # inforation.
        if re.search(CHARGEN_TXT, sample_txt):
          self.sample_txt=CHARGEN_TXT
          self.test_cli=['chargen {0} {1}'.format(len(CHARGEN_TXT), num_ch_exp)]
          self.use_chargen=True    # Execute only once.

      # Get a sample text
      if not self.use_chargen:
        self.test_cli=test_prof['default_cli']

        self.flushOutput()
        for cli in self.test_cli:
          self.run_command(cli)

        self.sample_txt=self.get_output()

      self.logger.debug("Test Plan on %s:" % self.port)
      self.logger.debug("  run %s for %d seconds" %           \
                    (' ,'.join(self.test_cli), duration))
    finally:
      self.close()

  def stress_test_expire(self):
    """ """
    self.test_expired=True

  def stress_test(self):
    """
    """
    self.open()
    self.flushOutput()

    self.test_cli_actual_count=0

    if self.use_chargen:
        for cli in self.test_cli:
          self.run_command(cli)
        self.test_cli_actual_count=1

        while not self.test_expired:
          self.result_txt+=self.get_output()      # TODO(Use thread read)
    else:
      while not self.test_expired:
        for cli in self.test_cli:
          self.run_command(cli)
        self.test_cli_actual_count+=1

        self.result_txt+=self.get_output()      # TODO(Use thread read)

  def start_test(self, duration):
    """ """
    self.test_expired=False

    self.test_thread=threading.Thread(target=self.stress_test)
    self.test_thread.start()

    t=threading.Timer(duration, self.stress_test_expire)
    t.start()

  def wait_for_test(self):
    """ """
    self.test_thread.join()
    self.logger.info("Test on %s is done." % self.port)


class UartStressTest(object):
  """UART stress tester
  """
  serials={}            # Serial objects

  def __init__(self, ports, duration=DURATION, use_chargen=False):
    """Setup UART stress tester
    Args:
        ports =
        duration =
    """
    self.ports=ports

    if duration <= 0:
        raise UartStressTestError("Input error: duration is not positive.")
    self.duration=duration

    self.use_chargen=use_chargen

    # Logging setup
    self.logger=logging.getLogger(type(self).__name__)

    # Create serial object for each device
    for port in self.ports:
      self.serials[port]=UartSerial(port=port)

    atexit.register(self.cleanup)

  def cleanup(self):
    """ Close all serial device """
    for port, ser in self.serials.items():
      self.logger.debug("Closing %s ..." % port)
      del(ser)
      self.logger.debug("%s is closed" % port)

  def prepare(self):
    """   """
    for port, ser in self.serials.items():
      self.logger.info("Preparing %s ..." % port)

      ser.test_configure(self.duration,
                         use_chargen=self.use_chargen)

      self.logger.info("%s is ready to test." % port)

  def output_result(self):
    """  """
    for port, ser in self.serials.items():
      self.logger.debug('%s: result\n %s' % (port, ser.result_txt))

  def run(self):
    """  """
    self.logger.info('Test starts.')
    self.prepare()

    for port, ser in self.serials.items():
      ser.start_test(self.duration)

    for port, ser in self.serials.items():
      ser.wait_for_test()

    # Print the result.
    self.output_result()
    self.logger.info('Test done.')


def parse_args(cmdline):
  """Parse command line arguments.

  Args:
    comdline: list, comdline to be parsed
  Returns:
    tuple (options, args) where args is a list of cmdline arguments that the
  parser was unable to match i.e. they're servod controls, not options.
  """
  description=( '%(prog)s repeats sending a uart console command\n'
      'to each UART device for a given time, and check if output\n'
      'has any missing characters.\n\n'
      'Examples:\n'
      '    %(prog)s /dev/ttyUSB2 --time 3600\n'
      '    %(prog)s /dev/ttyUSB1 /dev/ttyUSB2 --debug\n'
      )

  parser=argparse.ArgumentParser(description=description,
                                formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('port', type=str, nargs="*",
                      help='UART device path to test')
  parser.add_argument('-c', '--chargen', action='store_true', default=False,
                      help='test with chargen command')
  parser.add_argument('-d', '--debug', action='store_true', default=False,
                      help='enable debug messages')
  parser.add_argument('-t', '--time', type=int,
                      help='Test duration in second', default=300)
  return parser.parse_known_args(cmdline)


def main():
  try:
    """Main function wrapper"""
    (options, args)=parse_args(sys.argv[1:])

    # Set Log format
    log_format='%(asctime)s %(levelname)-6s | %(name)-16s'
    date_format='%Y-%m-%d %H:%M:%S'
    if options.debug:
      log_format+=' | %(filename)s:%(lineno)4d:%(funcName)-16s'
      loglevel=logging.DEBUG
    else:
      loglevel=logging.INFO
    log_format+=' | %(message)s'

    logging.basicConfig(level=loglevel, format=log_format, datefmt=date_format)

    # Create a UartStressTest object
    utest=UartStressTest(ports=options.port, duration=options.time,
                           use_chargen=options.chargen)
    # Run
    utest.run()

  except KeyboardInterrupt:
    sys.exit(0)
  except UartStressTestError as e:
    print('Error: ', str(e))
    sys.exit(1)

if __name__=='__main__':
  main()

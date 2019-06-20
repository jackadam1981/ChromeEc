#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# ChromeOS UART Stress Test
#
# This tester either runs the command 'chargen' once or runs some UART commands
# several times to each UART device, captures the output, and compares the
# output against the expected output to check any characters lost.
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
import serial
import sys
import tempfile
import threading
import time

DURATION = 30
BAUDRATE = 115200
KERNEL_USERNAME = 'root'
KERNEL_PASSWORD = 'test0000'
CHARGEN_TXT = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
CR = '\r'
LF = '\n'


class UartStressTestError(Exception):
  """Exception for Uart Stress Test Error"""
  pass


class UartSerial(object):
  """Test Object for a single UART serial device"""
  TEST_PROFILE_DICT = (
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
      # CR50
      {
        'prompt':'> ',
        'device_type':'CR50',
        'default_cli':[ 'waitms 0', 'pinmux' ],
      },
      # EC
      {
        'prompt':'> ',
        'device_type':'EC',
        'default_cli':[ 'waitms 0', 'gpioget' ],
      },
     )

  def __init__(self, port, duration, timeout = 1, parity = serial.PARITY_EVEN,
               baudrate = BAUDRATE, use_chargen = False,
               result_dir = ''):
    """
    Args:
      port: UART device path. e.g. /dev/ttyUSB0
      duration: Time to test, in seconds
      timeout: Read timeout value.
      parity: Parity checking. e.g. serial.PARITY_NONE or serial.PARITY_EVEN.
      baudrate: Baud rate such as 9600 or 115200.
      use_chargen: True if chargen should be used for workload generation.
      result_dir: Directory path to save the test files.
    """
    self.serial = serial.Serial()
    self.serial.port = port
    self.serial.timeout = timeout
    self.serial.parity = parity
    self.serial.baudrate = baudrate

    self.duration = duration
    self.result_dir = result_dir
    self.use_chargen = use_chargen

    self.logger = logging.getLogger(type(self).__name__)

    self.cleanup_cli = []
    self.test_cli = []
    self.test_iteration = 0
    self.sample_txt = ''
    self.test_expired = False
    self.result_path = ''
    self.eol = CR+LF

    atexit.register(self.cleanup)

  def run_command(self, command_lines, delay = 1):
    """
    Args:
      command_lines: list of commands to run.
      delay: delay after a command in second
    """
    for cli in command_lines:
      self.serial.write(cli + self.eol)
      self.serial.flush()
      if delay:
        time.sleep(delay)

  def cleanup(self):
    """Before termination, clean up the UART device."""
    port = self.serial.port
    self.logger.debug('Closing ' + port + ' ...')

    try:
      self.serial.open()
      # Restore display channel status (EC/CR50) or log out (kernel)
      self.run_command(self.cleanup_cli)
      self.serial.close()
    except:
      pass
    finally:
      self.logger.debug(port + ' is closed.')

  def get_output(self):
    """Capture the UART output

    Returns: the text from UART output.
    """
    uart_output = ''
    while self.serial.inWaiting() > 0:
      uart_output += self.serial.read(1)

    # Remove CR before returning the captured text.
    return uart_output.replace(CR, '')

  def prepare(self):
    """Prepare the test:
        Identify the type of UART device (EC or Kernel?), then
        decide what kind of commands to use to generate stress loads.

    Raises: UartStressTestError if UART source cannot not identified.
    """
    try:
      self.serial.open()

      # Prepare the device for test
      self.serial.flushInput()
      self.serial.flushOutput()

      tmp_txt = self.get_output()      # drain data

      # Give a line feed, and capture the prompt text
      self.run_command([''])
      prompt_txt = self.get_output()

      # Detect the device source: EC or AP?
      # Detect if the device is kernel or EC console based on the captured.
      for test_prof in self.TEST_PROFILE_DICT:
        if test_prof['prompt'] in prompt_txt:
          if test_prof['device_type'] == 'kernel':
            break

          self.run_command(['help'])
          tmp_txt = self.get_output()
          if test_prof['default_cli'][1] in tmp_txt:
            break
      else:
        # No prompt patterns were found. UART seems not responding or in
        # an undesirable status.
        self.logger.error(self.serial.port + ' is not identified: ' +
                          prompt_txt)
        raise UartStressTestError()

      self.logger.debug(self.serial.port + ' is ' + test_prof['device_type'] +
                        ' UART.')

      # Either login to kernel or run some commands to prepare the device
      # for test
      if test_prof['device_type'] == 'kernel':
        # Login to kernel
        if self.use_chargen:
          self.run_command([KERNEL_USERNAME, KERNEL_PASSWORD])
          self.cleanup_cli += ['logout']

        # Convert End of Line from CRLF to LF. Otherwise it will be regarded as
        # two line feed.
        self.eol = LF
      else:
        # Hush EC UART messages by turning off display channels.
        self.run_command(['chan save', 'chan 0'])
        self.cleanup_cli += ['chan restore']

      # Determine CLI to test
      # Initialize test command line and the frequency.
      tmp_txt = self.get_output()      # drain data before capture
      if self.use_chargen:
        # Check whether the command 'chargen' is available in the device.
        self.run_command(['chargen {0} {0}'.format(len(CHARGEN_TXT))])
        sample_txt = self.get_output()

        # If chargen command is available, then update the test command
        # inforation.
        if CHARGEN_TXT in sample_txt:
          self.sample_txt = CHARGEN_TXT
          self.num_ch_exp = int(self.serial.baudrate * self.duration / 10)
          self.test_cli = ['chargen {0} {1}'.format(len(CHARGEN_TXT),
                                                    self.num_ch_exp)]
        else:
          # Some EC might not have chargen. If it is the case, then use 'help'
          # command instead.
          self.use_chargen = False
          self.num_ch_exp = 0

      # If chargen command is not requested nor available, use a default command
      # instead.
      if not self.use_chargen:
        self.test_cli = test_prof['default_cli']
        self.run_command(self.test_cli)
        self.sample_txt = self.get_output()

      # Save the sample pattern into a file.
      fd, self.sample_path = tempfile.mkstemp(
                              dir = self.result_dir,
                              prefix = os.path.basename(self.serial.port) +
                                       '_sample_')
      with os.fdopen(fd, 'w') as tmp:
        tmp.write(self.sample_txt)

    except Exception as e:
      self.logger.error('Test Failed: ' + str(e))
      raise UartStressTestError(e)
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

      self.test_iteration = 0

      # Create a temp file to store the captured output
      fd, self.result_path = tempfile.mkstemp( dir = self.result_dir,
                                prefix = os.path.basename(self.serial.port) +
                                         '_result_')
      with os.fdopen(fd, 'w') as tmp:
        if self.use_chargen:
          # Run the command 'chargen', one time
          self.run_command(self.test_cli)
          self.test_iteration += 1

          # Keep capturing the output until the test timer is expired.
          while not self.test_expired:
            tmp.write(self.get_output())

        else:
          # Repeating UART commands in self.test_cli until the timer is expired.
          while not self.test_expired:
            # Cautions: 0 interval chokes up CR50 or EC.
            self.run_command(self.test_cli, 0.1)
            self.test_iteration += 1
            tmp.write(self.get_output())

        # Gather the rest of output.
        time.sleep(3)
        tmp.write(self.get_output())
    finally:
      self.serial.close()

  def start_test(self):
    """Start the test thread"""
    self.test_expired = False
    self.test_thread = threading.Thread(target = self.stress_test_thread)

    self.logger.info('Test on ' + self.serial.port + ' start.')
    self.test_thread.start()

    # After the test duration, set up the test terminal condition
    t = threading.Timer(self.duration, self.stress_test_thread_expire)
    t.start()

  def wait_test_done(self):
    """Wait until the test thread get done and join"""
    self.test_thread.join()

    self.logger.debug('Sample text is at ' + self.sample_path)
    self.logger.debug('Test iteration was %d times' % self.test_iteration)
    self.logger.info('Captured text is at ' + self.result_path)
    self.logger.info('Test on ' + self.serial.port + ' is done.')

  def get_result(self):
    """Display the result

    Returns:
      Integer = the number of lost character
    Raises:
      UartStressTestError: if the capture is corrupted.
    """
    if self.use_chargen:
      # If chargen command was used,
      char_expected = self.num_ch_exp
      # Find the pattern, trim a linefeed, and count the character from the
      # captured file.
      # Note: Chargen generates the result in one line.
      char_captured = int(os.popen('grep -s {0} {1} | tr -d "\n" |'
                                   ' wc -c'.format(self.sample_txt,
                                                   self.result_path)).read())
    else:
      # If 'help' command or other
      char_expected = self.test_iteration * len(self.sample_txt)
      # Count the character from the captured file.
      char_captured = int(os.popen('wc -c < ' + self.result_path).read())

    # If the character count is zero, then it means the capture failed.
    if char_expected == 0 :
      raise UartStressTestError('Test on ' + self.serial.port +
                                ' is not prepared properly.')

    # If more characters than expected are captured, it means some messages
    # from other than chargen are mixed. Stop processing further.
    if char_expected < char_captured :
      self.logger.error('UART output from ' + self.serial.port +
                                ' is corrupted.')
      raise UartStressTestError()

    # Get the count difference between the expected to the captured
    # as the number of lost character.
    char_lost = char_expected - char_captured
    self.logger.info('{0}: {1} char lost / {2} ({3} %), at {4} char/s.'.format(
                        self.serial.port, char_lost, char_expected,
                        char_lost * 100 / char_expected,
                        char_expected / self.duration))
    return char_lost


class UartStressTest(object):
  """UART stress tester"""

  def __init__(self, ports, duration, use_chargen = False):
    """Setup UART stress tester

    Args:
      ports: List of UART ports to test.
      duration: Time to keep running test in seconds.
      use_chargen: True if 'chargen' should be used for stress generation
                    False, otherwise.
    """
    self.ports = ports

    if duration <= 0:
        raise UartStressTestError('Input error: duration is not positive.')
    self.duration = duration

    self.use_chargen = use_chargen

    # Logging setup
    self.logger = logging.getLogger(type(self).__name__)

    # Create a temporary directory for result files.
    self.result_dir = tempfile.mkdtemp(prefix = type(self).__name__ + '_')

    # Create a serial object for each device
    self.serials = {}     # Serial objects
    for port in self.ports:
      self.serials[port] = UartSerial(port = port, duration = self.duration,
                                      use_chargen = self.use_chargen,
                                      result_dir = self.result_dir)

  def prepare(self):
    """Prepare the test for each UART port"""
    for port, ser in self.serials.items():
      self.logger.info('Preparing ' + port + ' ...' )
      ser.prepare()
      self.logger.info(port + ' is ready to test.')

  def print_result(self):
    """Display the test result for each UART port"""
    char_lost = 0
    for port, ser in self.serials.items():
      char_lost += ser.get_result()

    # If any characters are lost, then test fails.
    msg = 'lost {0} character(s) from the test.'.format(char_lost)
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
  description = ( '%(prog)s repeats sending a uart console command\n'
      'to each UART device for a given time, and check if output\n'
      'has any missing characters.\n\n'
      'Examples:\n'
      '    %(prog)s /dev/ttyUSB2 --time 3600\n'
      '    %(prog)s /dev/ttyUSB1 /dev/ttyUSB2 --debug\n'
      )

  parser = argparse.ArgumentParser(description = description,
                                formatter_class = argparse.RawTextHelpFormatter)
  parser.add_argument('port', type = str, nargs = "*",
                      help = 'UART device path to test')
  parser.add_argument('-c', '--chargen', action = 'store_true', default = False,
                      help = 'test with chargen command')
  parser.add_argument('-d', '--debug', action = 'store_true', default = False,
                      help = 'enable debug messages')
  parser.add_argument('-t', '--time', type = int,
                      help = 'Test duration in second', default = 300)
  return parser.parse_known_args(cmdline)


def main():
  """Main function wrapper"""
  try:
    (options, args) = parse_args(sys.argv[1:])

    # Set Log format
    log_format = '%(asctime)s %(levelname)-6s | %(name)-16s'
    date_format = '%Y-%m-%d %H:%M:%S'
    if options.debug:
      log_format += ' | %(filename)s:%(lineno)4d:%(funcName)-16s'
      loglevel = logging.DEBUG
    else:
      loglevel = logging.INFO
    log_format += ' | %(message)s'

    logging.basicConfig(level = loglevel, format = log_format,
                        datefmt = date_format)

    # Create a UartStressTest object
    utest = UartStressTest(options.port, options.time, options.chargen)
    utest.run()    # Run

  except KeyboardInterrupt:
    sys.exit(0)

  except UartStressTestError as e:
    print('Error: ', str(e))
    sys.exit(1)

if __name__ == '__main__':
  main()

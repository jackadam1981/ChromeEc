#!/usr/bin/env python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Flash PD PSU RW firmware over the USBPD comm channel using console."""

import array
import hashlib
import logging
import optparse
import os
import sys
import time

# TODO(tbroch): Discuss adding hdctools as an EC package RDEPENDS
from servo import client
from servo import multiservo


VERSION = '0.0.1'

# RW area is half of the 32-kB flash minus the hash storage area
MAX_FW_SIZE = 16 * 1024 - 32
# Hash of RW when erased (set to all F's)
ERASED_RW_HASH = 'd582e94d 0d12a61c 1199927e 5610c036 2e2870a9'


class FlashPDError(Exception):
  """Exception class for flash_pd utility."""


class FlashPD(client.ServoClient):
  """class to flash PD MCU.

  Attributes:
    _options: Values instance from optparse
  """
  _SLEEP_INTERVAL = 0.2

  def __init__(self, options):
    super(FlashPD, self).__init__(host=options.server, port=options.port)
    self._options = options

    if not self.get('ec_uart_pty'):
      raise FlashPDError('Unable to determine EC uart from servod')

    # initialize uart communication stream
    self.set('ec_uart_capture', 'on')

  def flash_command(self, cmd):
    cmd = 'pd flash %s' % cmd
    logging.debug('ec cmd: %s', cmd)
    self.set('ec_uart_cmd', cmd)
    rsp = ''
    still_time = self._options.timeout
    while 'DONE' not in rsp and still_time > 0:
      rsp += self.get('ec_uart_stream')
      time.sleep(self._SLEEP_INTERVAL)
      still_time -= self._SLEEP_INTERVAL

    logging.debug('cmd took %2.2f secs', self._options.timeout - still_time)
    logging.debug('ec rsp:\n%s', rsp)
    if 'DONE' not in rsp:
      raise FlashPDError('pd flash %s failed' % cmd)
    return rsp

  def __del__(self):
    self.set('ec_uart_capture', 'off')


def flash_pd(options):
  """Flash power delivery firmware."""

  with open(options.firmware) as fd:
    fw = fd.read()
    fw_size = len(fw)
    # Compute SHA-1 hash for the full (padded) RW firmware
    padded_fw = fw + '\xff' * (MAX_FW_SIZE - fw_size)
    sha = hashlib.sha1(padded_fw).digest()
    sha_str = ' '.join(['%08x' % (w) for w in array.array('I', sha)])

    # pad the firmware to a multiple of 6 U32
    if fw_size % 24:
      fw += '\xff'*(24 - fw_size % 24)
    words = array.array('I', fw)

  logging.info('Flashing %d bytes', fw_size)
  ec = FlashPD(options)
  # reset flashed hash to reboot in RO
  ec.flash_command('hash' + ' 00000000' * 5)
  # reboot in RO
  ec.flash_command('reboot')
  # delay to give time to reboot
  time.sleep(0.5)
  # erase all RW partition
  ec.flash_command('erase')

  # verify that erase was successful by reading hash of RW
  rsp = ec.flash_command('rw_hash')
  if ERASED_RW_HASH in rsp and 'DONE' in rsp:
    logging.info('Successfully erased flash.')
  else:
    raise FlashPDError('Erase failed')

  # write firmware content
  for i in xrange(len(words) / 6):
    chunk = words[i * 6: (i + 1)*6]
    ec.flash_command(' '.join(['%08x' % (w) for w in chunk]))

  # write new firmware hash
  ec.flash_command('hash ' + sha_str)
  # reboot in RW
  ec.flash_command('reboot')
  # delay for reboot
  time.sleep(0.2)
  # print out new version to prompt
  ec.flash_command('version')

  logging.info('Flashing DONE.')
  logging.info('SHA-1: %s', sha_str)


def parse_args():
  """Parse commandline arguments.

  Note, reads sys.argv directly

  Returns:
    options: dict of from optparse.parse_args().

  Raises:
    FlashPDError: If problems with arguments
  """
  description = (
      '%prog [<switch args>] <firmware.bin>'
      ''
      '%prog is a utility for flashing the USB-PD charger RW firmware over '
      'the USB-PD communication channel using PD MCU console commands.'
      )
  examples = (
      '\nExamples:\n'
      '   %prog firmware.bin\n'
      )
  parser = optparse.OptionParser(version='%prog ' + VERSION)
  parser.description = description
  parser.add_option('-d', '--debug', action='store_true', default=False,
                    help='enable debug messages.')
  parser.add_option('-s', '--server', help='host where servod is running',
                    default=client.DEFAULT_HOST)
  parser.add_option('-p', '--port', default=client.DEFAULT_PORT, type=int,
                    help='port servod is listening on.')
  parser.add_option('', '--timeout', default=5, type=int,
                    help='Timeout seconds to wait for console output.')
  multiservo.add_multiservo_parser_options(parser)

  parser.set_usage(parser.get_usage() + examples)
  (options, args) = parser.parse_args()

  # TODO(tbroch) Add this once we refactor module to ease use in scripts.
  if options.name:
    raise NotImplementedError('Multiservo support TBD')

  # Add after to enumerate options.firmware but outside 'help' generation
  parser.add_option('-f', '', action='store', type='string', dest='firmware')

  if len(args) != 1:
    raise FlashPDError('Must supply power delivery firmware to write.')

  options.firmware = args[0]
  if not os.path.exists(options.firmware):
    raise FlashPDError('Unable to find file %s' % options.firmware)

  fw_size = os.path.getsize(options.firmware)
  if fw_size > MAX_FW_SIZE:
    raise FlashPDError('Firmware too large %d/%d' % (fw_size, MAX_FW_SIZE))

  return options


def main_function():
  options = parse_args()

  loglevel = logging.INFO
  log_format = '%(asctime)s - %(name)s - %(levelname)s'
  if options.debug:
    loglevel = logging.DEBUG
    log_format += ' - %(filename)s:%(lineno)d:%(funcName)s'
  log_format += ' - %(message)s'
  logging.basicConfig(level=loglevel, format=log_format)

  flash_pd(options)


def main():
  """Main function wrapper to catch exceptions properly."""
  try:
    main_function()
  except KeyboardInterrupt:
    sys.exit(0)
  except FlashPDError as e:
    print 'Error: ', e.message
    sys.exit(1)

if __name__ == '__main__':
  main()

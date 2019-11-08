#!/usr/bin/env python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Flash Cr50 using cr50-rescue.

  Example:
    util/flash_cr50.py -p 9999 --image cr50.bin.prod
"""


import argparse
import logging
import os
import pprint
import random
import re
import shlex
import string
import subprocess
import sys
import threading
import time

VERSION = '0.0.1'

# Dictionary mapping the required setup to controls we can use to verify the
# setup is correct. The key is the type of thing needed for setup. The values
# are a list of controls with a regular expression with the required output.
REQUIRED_CONTROLS = {
  'type-c_servo_v4' : [
    'servo_v4_type:type-c',
    'servo_v4_role:\S+',
  ],
  'cr50_uart' : [
    'raw_cr50_uart_pty:\S+',
    'cr50_ec3po_interp_connect:\S+'
  ],
  'cr50_reset_odl' : [
    'cr50_reset_odl:\S+',
  ],
  'flex' : [
    'servo_type:.*servo_.[^4]',
  ]
}

# Dictionary of supported reset types for Cr50 reset. The keys are the type of
# reset. The keys are a list of requirements for the setup. The reset type
# strings must have corresponding run_$reset_type and recover_$reset_type
# functions in FlashCr50. The requirement strings must match something in the
# REQUIRED_CONTROLS dictionary above.
SUPPORTED_RESET_TYPES = {
  'battery_cutoff': [
      # Rescue is done through Cr50 uart, so we need a flex cable.
      'flex',
      # We need type c servo v4 to recover from battery_cutoff.
      'type-c_servo_v4',
      # Cr50 rescue is done through cr50 uart.
      'cr50_uart',
  ],
  'cr50_reset_odl': [
      # Rescue is done through Cr50 uart, so we need a flex cable.
      'flex',
      # cr50_reset_odl is used to hold cr50 in reset.
      'cr50_reset_odl',
      # Cr50 rescue is done through cr50 uart.
      'cr50_uart'
  ]
}

class FlashCr50Error(Exception):
  """Exception class for flash_cr50 utility."""


def run_cmd(cmd, raise_error=True):
  """Run the given command.

  Returns:
    The command output or the CalledProcessError if the command fails and
    raise_error is false.

  Raises:
    subprocess.CalledProcessError if the command fails and raise_error is True.
  """
  logging.debug('Running command: %s', cmd)
  split_cmd = shlex.split(cmd)
  try:
    return subprocess.check_output(split_cmd,stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError, e:
    if raise_error:
      raise
    return e

class Cr50Image():
  """Class to handle converting the cr50 rw image to hex format"""

  SUFFIX_LEN = 6
  RW_NAME = 'cr50.rw.'

  def __init__(self, image, artifacts_dir):
    """Create an image that can be used by cr50-rescue."""
    if not os.path.exists(image):
        raise FlashCr50Image('Could not find image: %s' % image)
    if not os.path.exists(artifacts_dir):
        raise FlashCr50Image('Directory does not exist: %s' % artifacts_dir)

    self._original_image = image
    self._artifacts_dir = artifacts_dir
    self._generate_file_names()
    self._save_hex_image()


  def _generate_file_names(self):
    """Create some filenames to use for image conversion artifacts."""
    suffix = ''.join([random.choice(string.ascii_letters) for i in
                      xrange(self.SUFFIX_LEN)])
    name = self.RW_NAME + suffix
    self._tmp_rw_bin = os.path.join(self._artifacts_dir, name + '.bin')
    self._tmp_rw_hex = os.path.join(self._artifacts_dir, name + '.hex')
    self._tmp_cr50_bin = os.path.join(self._artifacts_dir, name + '.orig.bin')


  def _save_hex_image(self):
    """Save the rw hex from the given image"""
    ext = os.path.splitext(self._original_image)[1]
    if ext == '.bin':
      self._save_hex_from_bin()
    elif ext == '.hex':
      self._save_original_hex()
    else:
      raise FlashCr50Error('Unsupported image format %r use .bin or .hex' % ext)


  def _save_original_hex(self):
    """The original image is .hex. Save it."""
    run_cmd('cp %s %s' % (self._original_image, self._tmp_rw_hex))


  def _save_hex_from_bin(self):
    """Convert the image from .bin to .hex."""
    run_cmd('cp %s %s' % (self._original_image, self._tmp_cr50_bin))
    run_cmd('dd if=%s of=%s skip=16384 count=233472 bs=1' %
            (self._tmp_cr50_bin, self._tmp_rw_bin))
    run_cmd('objcopy -I binary -O ihex --change-addresses 0x44000 %s %s' %
            (self._tmp_rw_bin, self._tmp_rw_hex))


  def __str__(self):
    """cr50-rescue uses the .hex file."""
    return self._tmp_rw_hex


class FlashCr50():
  """Class to flash cr50 through servo micro uart"""

  SHORT_WAIT = 3
  WAIT_FOR_UPDATE = 120
  RESCUE_CMD = 'cr50-rescue -v -i %s -d %s'


  def __init__(self, port, reset_type):
    """Make sure the setup supports the given reset_type."""
    self._rescue_thread = None
    self._port = port
    self._reset_type = reset_type
    self.verify_setup()
    self._original_watchdog_state = self.ccd_watchdog_enabled()


  def update(self, image):
    """Update the dut and cleanup."""
    try:
      self.run_update(image)
    finally:
      self.restore_state()


  def start_rescue_process(self, image):
    """Start cr50-rescue in a process, so we can kill it if it fails."""
    logging.info('Starting cr50-rescue')
    rescue_cmd = self.RESCUE_CMD % (image, self._raw_cr50_uart_pty)
    logging.info(rescue_cmd)
    self._rescue_process = subprocess.Popen(shlex.split(rescue_cmd))
    self._rescue_process.communicate()
    logging.info('Rescue Finished')


  def start_rescue_thread(self, image):
    """Start cr50-rescue."""
    self._rescue_thread = threading.Thread(target=self.start_rescue_process,
                                           args=[image])
    self._rescue_thread.start()


  def run_update(self, image):
    """Run the Update"""
    self._image = image

    logging.info('Using cr50-rescue to update cr50')

    # Enter reset before starting rescue, so any extra cr50 messages won't
    # interfere with cr50-rescue.
    self.enter_cr50_reset()

    # Disconnect EC3PO, so it doesn't interfere with rescue.
    self.dut_control('cr50_ec3po_interp_connect:off')
    time.sleep(self.SHORT_WAIT)

    self.start_rescue_thread(image)

    # Resume from cr50 reset.
    self.exit_cr50_reset()

    self._rescue_thread.join(self.WAIT_FOR_UPDATE)

    # Reconnect EC3PO, so we can use cr50 dut-control commands again.
    self.dut_control('cr50_ec3po_interp_connect:on')
    # Give cr50 some time to recover, so we can check the version.
    time.sleep(self.SHORT_WAIT)
    logging.info('cr50_version:%s', self.dut_control('cr50_version')[1])


  def restore_state(self):
    """Try to get the device out of reset and restore all controls"""
    logging.info('Cleaning up')
    self.restore_control('cr50_ec3po_interp_connect')
    self.restore_ec_uart()
    self.restore_watchdog()
    self.restore_control('servo_v4_role')
    self.cleanup_rescue_thread()


  def cleanup_rescue_thread(self):
    """Cleanup the rescue process and handle any errors."""
    if not self._rescue_thread:
      return
    if self._rescue_thread.is_alive():
      logging.info('Killing cr50-rescue process')
      self._rescue_process.terminate()
      self._rescue_thread.join()

    self._rescue_thread = None
    if self._rescue_process.returncode:
      logging.info('cr50-rescue failed.')
      logging.info('stderr: %s', self._rescue_process.stderr)
      logging.info('stdout: %s', self._rescue_process.stdout)
      logging.info('returncode: %s', self._rescue_process.returncode)
      raise FlashCr50Error('cr50-rescue failed (%d)' %
                           self._rescue_process.returncode)


  def restore_watchdog(self):
    """Restore the CCD watchdog state."""
    self.enable_ccd_watchdog(self._original_watchdog_state)


  def restore_control(self, control):
    """Restore the control setting, if we saved it."""
    setting = getattr(self, control, None)
    if setting is None:
      return
    self.dut_control('%s:%s' % (control, setting))

  def restore_ec_uart(self):
    """It's possible the battery is still cutoff. Toggle servo role recover."""
    self.dut_control('servo_v4_role:snk', raise_error=False)
    self.dut_control('servo_v4_role:src', raise_error=False)


  def dut_control(self, cmd, raise_error=True):
    """Run dut control commands

    Args:
      cmd - the command to run
      raise_error - Raise the command error if True.

    Returns:
      (exit_status, response string) - The exit_status will be non-zero if the
      command failed and raise_error is False.

    Raises:
      subprocess.CalledProcessError if the command fails and raise_error is
      true.
    """
    response = run_cmd('dut-control %s -p %s' % (cmd, self._port), raise_error)
    logging.debug(response)
    if isinstance(response, str):
      return 0, response.strip().split(':', 1)[-1]
    else:
      return response.returncode, response.output.rsplit(':', 1)[-1].strip()

  def ccd_watchdog_enabled(self):
    """Return True if servod is monitoring ccd"""
    if 'ccd_cr50' not in self._servo_type:
      return False
    watchdog_state = self.dut_control('watchdog')[1]
    logging.debug(watchdog_state)
    return not re.search('ccd:.*disconnect ok', watchdog_state)


  def enable_ccd_watchdog(self, enable):
    """Control the watchdog, so servo wont die when cr50 is in reset"""
    if 'ccd_cr50' not in self._servo_type:
      logging.debug('Servo is not watching ccd device.')
      return

    if enable:
      self.dut_control('watchdog_add:ccd')
    else:
      self.dut_control('watchdog_remove:ccd')

    if self.ccd_watchdog_enabled() != enable:
      raise FlashCr50Error('Could not %sable ccd watchdog' %
                           ('en' if enable else 'dis'))


  def verify_setup(self):
    """Verify the setup has all required controls to flash cr50.

    Raises:
      FlashCr50Error if something is wrong with the setup.
    """
    # If this failed before and didn't cleanup correctly, the device may be
    # cutoff. Try to set the servo_v4_role to recover the device before checking
    # the device state.
    self.dut_control('servo_v4_role:src', raise_error=False)

    setup_issues = []
    if self._reset_type not in SUPPORTED_RESET_TYPES.keys():
      raise FlashCr50Error('Unsupported reset type %r' % self._reset_type)

    requirement_categories = SUPPORTED_RESET_TYPES[self._reset_type]

    logging.info('Requirements for %s: %s', self._reset_type,
                 pprint.pformat(requirement_categories))

    # Get the specific control requirements for the necessary categories.
    required_controls = []
    for category in requirement_categories:
      required_controls.extend(REQUIRED_CONTROLS[category])

    logging.debug('Required controls for %r:\n%s', self._reset_type,
                  pprint.pformat(required_controls))
    # Check the setup has all required controls in the correct state.
    for required_control in required_controls:
      control, exp_response = required_control.split(':')
      returncode, response = self.dut_control(control, False)
      logging.debug('%s: got %s expect %s', control, response, exp_response)
      match = re.search(exp_response, response)
      if returncode:
        setup_issues.append('%s: %s' % (control, response))
      elif not match:
        setup_issues.append('%s: need %s running %s' % (control, exp_response,
                                                        response))
      logging.debug('matched control: %s:%s', control, match.string)

      # Save controls, so we can restore them during cleanup if necessary.
      setattr(self, '_' + control, response)

    if setup_issues:
      raise FlashCr50Error('Cannot run update with %s due to setup issues: %s' %
                           (self._reset_type, setup_issues))
    logging.info('Device Setup: ok')
    logging.info('Reset Method: %s', self._reset_type)


  def enter_cr50_reset(self):
    """Enter Cr50 reset.

    Each reset type must have a run_$reset_type function defined. Cr50 doesn't
    have to enter reset in this function. It just needs to do whatever setup
    is necessary for the recover_$reset_type function.
    """
    logging.info('Enter cr50 reset through %s', self._reset_type)
    # CCD will disappear while Cr50 is in reset. Disable the CCd watchdog, so
    # servo won't die while cr50 is in reset.
    self.enable_ccd_watchdog(False)

    getattr(self, 'run_' + self._reset_type)()


  def exit_cr50_reset(self):
    """Exit Cr50 reset.

    Each reset type must have a recover_from_$reset_type function defined. Cr50
    has to come out of some form of hard reset during this function for rescue
    to work. Uart is disabled on deep sleep recovery, so deep sleep is not a
    valid reset.
    """
    logging.info('Releasing cr50 from %s', self._reset_type)
    getattr(self, 'recover_from_' + self._reset_type)()


  # cr50_reset_odl reset functions
  def run_cr50_reset_odl(self):
    """Use the Cr50 reset signal to hold Cr50 in reset."""
    logging.info('cr50_reset_odl:on')
    self.dut_control('cr50_reset_odl:on')


  def recover_from_cr50_reset_odl(self):
    """Release the reset signal."""
    logging.info('cr50_reset_odl:off')
    self.dut_control('cr50_reset_odl:off')


  # Battery cutoff reset functions
  def run_battery_cutoff(self):
    """Use EC commands to cutoff the battery."""
    self.dut_control('servo_v4_role:snk')

    if self.dut_control('ec_board', raise_error=False)[0]:
      logging.warn('EC is unresponsive. Cutoff may not work.')

    self.dut_control('ec_uart_cmd:cutoff')
    time.sleep(self.SHORT_WAIT)
    self.dut_control('ec_uart_cmd:reboot')
    time.sleep(self.SHORT_WAIT)

    if not self.dut_control('ec_board', raise_error=False)[0]:
      raise flashCr50Error('EC still responsive after cutoff')
    logging.info('Device is cutoff')


  def recover_from_battery_cutoff(self):
    """Connect power using servo v4 to recover from cutoff."""
    time.sleep(self.SHORT_WAIT)
    logging.info('"Connecting" adapter')
    self.dut_control('servo_v4_role:src')
    time.sleep(self.SHORT_WAIT)


def parse_args():
  """Parse commandline arguments.

  Note, reads sys.argv directly

  Returns:
    options : dict of from optparse.parse_args().
  """
  desc = 'A utility to flash cr50 through servo uart.'
  usage = 'flash_cr50.py -i $IMAGE -p $SERVO_PORT [-r $RESET_METHOD]'
  parser = argparse.ArgumentParser(VERSION, usage=usage, description=desc)
  parser.add_argument('-d', '--debug', action='store_true', default=False,
                      help='enable debug messages.')
  parser.add_argument('-p', '--port', type=str,
                      help='port servod is listening on.', required=True)
  parser.add_argument('-i', '--image', type=str, help='Cr50 image.',
                      required=True )
  parser.add_argument('-a', '--artifacts_dir', default='/tmp', type=str,
                      help='Location to store artifacts')
  parser.add_argument('-r', '--reset_type', default='battery_cutoff', type=str,
                      help='Method for cr50 reset. One of %s' %
                      SUPPORTED_RESET_TYPES.keys())
  return parser.parse_args()

def main_function():
  args = parse_args()

  loglevel = logging.INFO
  log_format = '%(asctime)s - %(levelname)s'
  if args.debug:
    loglevel = logging.DEBUG
    log_format += ' - %(filename)s:%(lineno)d:%(funcName)s'
  log_format += ' - %(message)s'
  logging.basicConfig(level=loglevel, format=log_format)
  logging.info('Updating to %s', args.image)
  image = Cr50Image(args.image, args.artifacts_dir)
  flash_cr50 = FlashCr50(args.port, args.reset_type)
  flash_cr50.update(image)

def main():
  """Main function wrapper to catch exceptions properly."""
  try:
    main_function()
  except KeyboardInterrupt:
    sys.exit(0)

if __name__ == '__main__':
  main()

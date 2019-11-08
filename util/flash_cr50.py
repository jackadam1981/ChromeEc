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
import select
import shlex
import string
import subprocess
import sys
import threading
import time

VERSION = '0.0.1'

CR50_FIRMWARE = '/opt/google/cr50/firmware/cr50.bin.'
RELEASE_PATHS = {
  'prepvt' : CR50_FIRMWARE + 'prepvt',
  'prod' : CR50_FIRMWARE + 'prod'
}
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
# List of supported cr50 reset types.
SUPPORTED_RESETS = [ 'battery_cutoff', 'cr50_reset_odl', 'manual_reset' ]


class FlashCr50Error(Exception):
  """Exception class for flash_cr50 utility."""

def run_cmd(cmd, raise_error=True):
  """Run the given command.

  Returns:
    (exit_status, The command output)

  Raises:
    The command error if the command fails and raise_error is True.
  """
  logging.debug('%s', cmd)
  split_cmd = shlex.split(cmd)
  try:
    output = subprocess.check_output(split_cmd,stderr=subprocess.STDOUT).strip()
    return 0, output
  except subprocess.CalledProcessError, e:
    if raise_error:
      raise
    return e.returncode, e.output.strip()
  except OSError, e:
    if raise_error:
      raise
    return e.errno, e.strerror



class Cr50RescueImage(object):
  """Class to handle converting the rw .hex from the given image."""

  SUFFIX_LEN = 6
  RW_NAME = 'cr50.rw.'

  def __init__(self, image, artifacts_dir):
    """Create an image that can be used by cr50-rescue."""
    if not os.path.exists(artifacts_dir):
        raise FlashCr50Error('Directory does not exist: %s' % artifacts_dir)

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
    if ext == '.bin' or '.bin.' in self._original_image:
      self._save_hex_from_bin()
    elif ext == '.hex' or '.hex.' in self._original_image:
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


  def get_path(self):
    """cr50-rescue uses the .hex file."""
    return self._tmp_rw_hex


class Servo(object):
  """Class to interact with servo."""

  SHORT_WAIT = 3

  def __init__(self, port):
    """Initialize servo class."""
    self._port = port

  def dut_control(self, cmd, raise_error=True, wait=False):
    """Run dut control commands

    Args:
      cmd - the command to run
      raise_error - Raise the command error if True.
      wait: If True, wait SHORT_WAIT seconds after running the command

    Returns:
      (exit_status, output string) - The exit_status will be non-zero if the
      command failed and raise_error is False.

    Raises:
      subprocess.CalledProcessError if the command fails and raise_error is
      true.
    """
    exit_status, output = run_cmd('dut-control %s -p %s' % (cmd, self._port),
                          raise_error)
    logging.debug(output)
    if wait:
      time.sleep(self.SHORT_WAIT)
    return exit_status, output.split(':', 1)[-1]


  def get_raw_cr50_pty(self):
    """Return the raw cr50 pty and disable ec3po, so it can be used."""
    # Disconnect EC3PO, so it doesn't interfere with rescue.
    self.dut_control('cr50_ec3po_interp_connect:off', wait=True)
    return self.dut_control('raw_cr50_uart_pty')[1]


  def get_cr50_version(self):
    """Return the raw cr50 pty and disable ec3po, so it can be used."""
    # Make sure ec3po is enabled, so we can get the version.
    self.dut_control('cr50_ec3po_interp_connect:on', wait=True)
    return self.dut_control('cr50_version')[1]


class Cr50Reset(object):
  """Class to enter and exit cr50 reset."""

  # A list of requirements for the setup. The requirement strings must match
  # something in the REQUIRED_CONTROLS dictionary.
  REQUIRED_SETUP = []


  def __init__(self, servo, name):
    """Make sure the setup supports the given reset_type."""
    self._servo = servo
    self._reset_name = name
    self.verify_setup()
    self._original_watchdog_state = self.ccd_watchdog_enabled()


  def verify_setup(self):
    """Verify the setup has all required controls to flash cr50.

    Raises:
      FlashCr50Error if something is wrong with the setup.
    """
    # If this failed before and didn't cleanup correctly, the device may be
    # cutoff. Try to set the servo_v4_role to recover the device before checking
    # the device state.
    self._servo.dut_control('servo_v4_role:src', raise_error=False)

    logging.info('Requirements for %s: %s', self._reset_name,
                 pprint.pformat(self.REQUIRED_SETUP))

    # Get the specific control requirements for the necessary categories.
    required_controls = []
    for category in self.REQUIRED_SETUP:
      required_controls.extend(REQUIRED_CONTROLS[category])

    logging.debug('Required controls for %r:\n%s', self._reset_name,
                  pprint.pformat(required_controls))
    setup_issues = []
    # Check the setup has all required controls in the correct state.
    for required_control in required_controls:
      control, exp_response = required_control.split(':')
      returncode, output = self._servo.dut_control(control, False)
      logging.debug('%s: got %s expect %s', control, output, exp_response)
      match = re.search(exp_response, output)
      if returncode:
        setup_issues.append('%s: %s' % (control, output))
      elif not match:
        setup_issues.append('%s: need %s running %s' % (control, exp_response,
                                                        output))
      logging.debug('matched control: %s:%s', control, match.string)

      # Save controls, so we can restore them during cleanup if necessary.
      setattr(self, '_' + control, output)

    if setup_issues:
      raise FlashCr50Error('Cannot run update with %s due to setup issues: %s' %
                           (self._reset_name, setup_issues))
    logging.info('Device Setup: ok')
    logging.info('Reset Method: %s', self._reset_name)


  def cleanup(self):
    """Try to get the device out of reset and restore all controls."""
    logging.info('Cleaning up')
    self.restore_control('cr50_ec3po_interp_connect')

    # Toggle the servo v4 role if possible to try and get the device out of
    # cutoff.
    self._servo.dut_control('servo_v4_role:snk', raise_error=False)
    self._servo.dut_control('servo_v4_role:src', raise_error=False)
    self.restore_control('servo_v4_role')

    # Restore the ccd watchdog.
    self.enable_ccd_watchdog(self._original_watchdog_state)


  def restore_control(self, control):
    """Restore the control setting, if we saved it."""
    setting = getattr(self, control, None)
    if setting is None:
      return
    self._servo.dut_control('%s:%s' % (control, setting))


  def ccd_watchdog_enabled(self):
    """Return True if servod is monitoring ccd"""
    if 'ccd_cr50' not in self._servo_type:
      return False
    watchdog_state = self._servo.dut_control('watchdog')[1]
    logging.debug(watchdog_state)
    return not re.search('ccd:.*disconnect ok', watchdog_state)


  def enable_ccd_watchdog(self, enable):
    """Control the watchdog, so servo wont die when cr50 is in reset"""
    if 'ccd_cr50' not in self._servo_type:
      logging.debug('Servo is not watching ccd device.')
      return

    if enable:
      self._servo.dut_control('watchdog_add:ccd')
    else:
      self._servo.dut_control('watchdog_remove:ccd')

    if self.ccd_watchdog_enabled() != enable:
      raise FlashCr50Error('Could not %sable ccd watchdog' %
                           ('en' if enable else 'dis'))


  def enter_reset(self):
    """Disable the watchdog then put cr50 into reset."""
    logging.info('Using %r to enter reset', self._reset_name)
    # Disable the CCD watchdog before putting servo into reset otherwise servo
    # will die in the middle of flashing cr50.
    self.enable_ccd_watchdog(False)
    try:
      self.run_reset()
    except Exception, e:
      logging.warn('%s enter reset failed: %s', self._reset_name, str(e))
      raise


  def exit_reset(self):
    """Exit cr50 reset."""
    logging.info('Recovering from %s', self._reset_name)
    try:
      self.recover_from_reset()
    except Exception, e:
      logging.warn('%s exit reset failed: %s', self._reset_name, str(e))
      raise


  def run_reset(self):
    """Start the cr50 reset process.

    Cr50 doesn't have to enter reset in this function. It just needs to do
    whatever setup is necessary for the exit reset function.
    """
    raise NotImplementedError()


  def recover_from_reset(self):
    """Recover from Cr50 reset.

    Cr50 has to hard or power-on reset during this function for rescue to work.
    Uart is disabled on deep sleep recovery, so deep sleep is not a valid reset.
    """
    raise NotImplementedError()


class Cr50ResetODLReset(Cr50Reset):
  """Class for using the servo cr50_reset_odl to reset cr50."""

  REQUIRED_SETUP  = [
      # Rescue is done through Cr50 uart, so we need a flex cable.
      'flex',
      # cr50_reset_odl is used to hold cr50 in reset.
      'cr50_reset_odl',
      # Cr50 rescue is done through cr50 uart.
      'cr50_uart'
  ]

  def cleanup(self):
    """Use the Cr50 reset signal to hold Cr50 in reset."""
    try:
      self.restore_control('cr50_reset_odl')
    finally:
      super(Cr50ResetODLReset, self).cleanup()

  def run_reset(self):
    """Use the Cr50 reset signal to hold Cr50 in reset."""
    logging.info('cr50_reset_odl:on')
    self._servo.dut_control('cr50_reset_odl:on')


  def recover_from_reset(self):
    """Release the reset signal."""
    logging.info('cr50_reset_odl:off')
    self._servo.dut_control('cr50_reset_odl:off')


class BatteryCutoffReset(Cr50Reset):
  """Class for using a battery cutoff through EC commands to reset cr50."""

  REQUIRED_SETUP  = [
      # Rescue is done through Cr50 uart, so we need a flex cable.
      'flex',
      # We need type c servo v4 to recover from battery_cutoff.
      'type-c_servo_v4',
      # Cr50 rescue is done through cr50 uart.
      'cr50_uart',
  ]

  def run_reset(self):
    """Use EC commands to cutoff the battery."""
    self._servo.dut_control('servo_v4_role:snk')

    if self._servo.dut_control('ec_board', raise_error=False)[0]:
      logging.warn('EC is unresponsive. Cutoff may not work.')

    self._servo.dut_control('ec_uart_cmd:cutoff', raise_error=False,
                            wait=True)
    self._servo.dut_control('ec_uart_cmd:reboot', raise_error=False,
                            wait=True)

    if not self._servo.dut_control('ec_board', raise_error=False)[0]:
      raise flashCr50Error('EC still responsive after cutoff')
    logging.info('Device is cutoff')


  def recover_from_reset(self):
    """Connect power using servo v4 to recover from cutoff."""
    logging.info('"Connecting" adapter')
    self._servo.dut_control('servo_v4_role:src', wait=True)


class ManualReset(Cr50Reset):
  """Class for using a manual reset to reset Cr50."""
  REQUIRED_SETUP  = [
      # Rescue is done through Cr50 uart, so we need a flex cable.
      'flex',
      # Cr50 rescue is done through cr50 uart.
      'cr50_uart'
  ]

  PROMPT_WAIT = 5
  USER_RESET_TIMEOUT = 60

  def run_reset(self):
    """Nothing to do. User will reset cr50."""
    pass


  def recover_from_reset(self):
    """Wait for the user to reset cr50."""
    end_time = time.time() + self.USER_RESET_TIMEOUT
    while time.time() < end_time:
      logging.info('Press enter after you reset cr50')
      user_input = select.select([sys.stdin], [], [], self.PROMPT_WAIT)[0]
      if user_input:
        logging.info('User reset done')
        return
    logging.warn('User input timeout: assuming cr50 reset')

class FlashCr50(object):
  """Class for updating cr50."""
  NAME = 'FlashCr50'
  PACKAGE = ''
  DEFAULT_UPDATER = ''

  def __init__(self, cmd):
    """Verify the update command exists."""
    updater = self._get_updater(cmd)
    if not updater:
      emerge_msg = ('Try emerging ' + self.PACKAGE) if self.PACKAGE else ''
      raise FlashCr50Error('Could not find %s command.%s' % (self, emerge_msg))
    self._updater = updater


  def _get_updater(self, cmd):
    """Find a valid updater."""
    if not self._test_updater_cmd(cmd):
      return cmd

    if (self.DEFAULT_UPDATER and
        not self._test_updater_cmd(self.DEFAULT_UPDATER)):
      logging.debug('%r failed using %r to update.', cmd, self.DEFAULT_UPDATER)
      return self.DEFAULT_UPDATER
    return None


  def _test_updater_cmd(self, cmd):
    """Verify the updater command.

    Returns:
      non-zero status if the command failed.
    """
    logging.debug('Testing update command %r.', cmd)
    exit_status, output = run_cmd('%s -h' % cmd, raise_error=False)
    if 'Usage' in output:
      return 0
    if exit_status:
      logging.debug('Could not run %r (%s): %s', cmd, exit_status, output)
    return exit_status

  def update(self, image):
    """Update cr50."""
    raise NotImplementedError()

  def __str__(self):
    """Use the updater name for the tostring."""
    return self.NAME


  def __repr__(self):
    """Use the updater name for the tostring."""
    return str(self)


class GsctoolUpdater(FlashCr50):
  """Class to flash cr50 using gsctool."""
  NAME = 'gsctool'
  PACKAGE = 'ec-utils'
  DEFAULT_UPDATER = '/usr/sbin/gsctool'

  # Common failures exit with this status. Use STANDARD_ERRORS to map the
  # exit status to reasons for the failure.
  STANDARD_ERROR_REGEX = 'Error: status (\S+)'
  STANDARD_ERRORS = {
    '0x8' : 'Rejected image with old header.',
    '0x9' : 'Update too soon.',
    '0xc' : 'Board id mismatch',
  }


  def __init__(self, cmd, serial):
    """Generate the gsctool command."""
    super(GsctoolUpdater, self).__init__(cmd)
    self._gsctool_cmd = self._updater
    if serial:
      self._gsctool_cmd = ' -n %s' % serial


  def update(self, image):
    """Use gsctool to update cr50."""
    exit_status, output = run_cmd('%s %s' % (self._gsctool_cmd, image),
                                  raise_error=False)
    logging.debug('gsctool output (%d):\n %s', exit_status,  output)
    if not exit_status or (exit_status == 1 and 'image updated' in output):
      logging.info('update ok')
      return
    if exit_status == 3:
      match = re.search(self.STANDARD_ERROR_REGEX, output)
      if match:
        update_error = match.group(1)
        logging.info('Update error %s', update_error)
        raise FlashCr50Error(self.STANDARD_ERRORS[update_error])
    raise FlashCr50Error('gsctool update error: %s' % output.splitlines()[-1])


class Cr50RescueUpdater(FlashCr50):
  """Class to flash cr50 through servo micro uart."""
  NAME = 'cr50-rescue'
  PACKAGE = 'cr50-utils'
  DEFAULT_UPDATER = '/usr/bin/cr50-rescue'

  WAIT_FOR_UPDATE = 120
  RESCUE_RESET_DELAY = 5
  RESCUE_ARGS = ' -v -i %s -d %s'


  def __init__(self, cmd, port, reset_type):
    """Initialize rescue thread and servo."""
    super(Cr50RescueUpdater, self).__init__(cmd)
    self._servo = Servo(port)
    self._rescue_cmd = self._updater + self.RESCUE_ARGS
    self._rescue_thread = None
    self._rescue_process = None
    self.set_cr50_reset(reset_type)


  def set_cr50_reset(self, reset_type):
    """Set the cr50 reset"""
    if reset_type and reset_type not in SUPPORTED_RESETS:
      raise FlashCr50Error('Unsupported cr50 reset method %r', reset_type)

    if reset_type == 'battery_cutoff':
      self._cr50_reset = BatteryCutoffReset(self._servo, reset_type)
    elif reset_type == 'cr50_reset_odl':
      self._cr50_reset = Cr50ResetODLReset(self._servo, reset_type)
    elif reset_type == 'manual_reset':
      self._cr50_reset = ManualReset(self._servo, reset_type)


  def update(self, image):
    """Update the dut and cleanup."""
    try:
      self.run_update(image)
    finally:
      self.restore_state()


  def start_rescue_process(self, image):
    """Start cr50-rescue in a process, so we can kill it if it fails."""
    pty = self._servo.get_raw_cr50_pty()
    rescue_cmd = self._rescue_cmd % (image, pty)
    logging.info('Starting cr50-rescue: %s', rescue_cmd)

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

    # Enter reset before starting rescue, so any extra cr50 messages won't
    # interfere with cr50-rescue.
    self._cr50_reset.enter_reset()

    self.start_rescue_thread(image)

    time.sleep(self.RESCUE_RESET_DELAY)
    # Resume from cr50 reset.
    self._cr50_reset.exit_reset()

    self._rescue_thread.join(self.WAIT_FOR_UPDATE)

    logging.info('cr50_version:%s', self._servo.get_cr50_version())


  def restore_state(self):
    """Try to get the device out of reset and restore all controls"""
    try:
      self._cr50_reset.cleanup()
    finally:
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

def parse_args():
  """Parse commandline arguments.

  Note, reads sys.argv directly

  Returns:
    options : dict of from optparse.parse_args().
  """
  desc = 'A utility to flash cr50 using gsctool or cr50-rescue.'
  usage = ('flash_cr50.py -i $IMAGE [ -c cr50-rescue -p $SERVO_PORT [ -r '
           '$RESET_METHOD]]')
  parser = argparse.ArgumentParser(VERSION, usage=usage, description=desc)
  parser.add_argument('-d', '--debug', action='store_true', default=False,
                      help='enable debug messages.')
  parser.add_argument('-i', '--image', type=str,
                      help='Cr50 image path or release type. Use %s to use the '
                           'current release.' %
                           (' or '.join(RELEASE_PATHS.keys())),
                      required=True )
  parser.add_argument('-c', '--update_cmd', type=str, default='gsctool',
                      help='Tool to update cr50. Either gsctool or cr50-rescue')
  parser.add_argument('-s', '--serial', type=str, default='',
                      help='serialnumber to pass to gsctool.')
  parser.add_argument('-p', '--port', type=str, default='',
                      help='port servod is listening on (required for rescue).')
  parser.add_argument('-r', '--reset_type', default='battery_cutoff', type=str,
                      help='Cr50 has to be reset during rescue. This is the '
                      'method for cr50 reset. One of %s' % SUPPORTED_RESETS)
  parser.add_argument('-a', '--artifacts_dir', default='/tmp', type=str,
                      help='Location to store artifacts')
  return parser.parse_args()


def main_function():
  args = parse_args()

  image = RELEASE_PATHS.get(args.image, args.image)
  if not os.path.exists(image):
      raise FlashCr50Error('Could not find image: %s' % image)

  loglevel = logging.INFO
  log_format = '%(asctime)s - %(levelname)7s'
  if args.debug:
    loglevel = logging.DEBUG
    log_format += ' - %(lineno)3d:%(funcName)-15s'
  log_format += ' - %(message)s'
  logging.basicConfig(level=loglevel, format=log_format)
  if 'cr50-rescue' in args.update_cmd:
    if not args.port:
      raise FlashCr50Error('Servo port is required for cr50 rescue')
    image = Cr50RescueImage(image, args.artifacts_dir).get_path()
    flash_cr50 = Cr50RescueUpdater(args.update_cmd, args.port, args.reset_type)
  if 'gsctool' in args.update_cmd:
    flash_cr50 = GsctoolUpdater(args.update_cmd, args.serial)

  logging.info('Using %r to update to %s', flash_cr50, image)
  flash_cr50.update(image)


def main():
  """Main function wrapper to catch exceptions properly."""
  try:
    main_function()
  except KeyboardInterrupt:
    sys.exit(0)

if __name__ == '__main__':
  main()

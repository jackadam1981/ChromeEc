# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for using lightweight console functions."""

import errno
import os
import subprocess
import time

import pty_driver
import stm32uart

logfile = None
testerlogfile = None


class TinyServoError(Exception):
  """Exceptions."""

def open_logfile(filename):
  """Open a file and create directory structure if needed."""
  if not os.path.exists(os.path.dirname(filename)):
    try:
      os.makedirs(os.path.dirname(filename))
    except OSError as exc:
      if exc.errno != errno.EEXIST:
        raise TinyServoError('Log', 'Cannot open logfile')
  return open(filename, 'a')

def finish_logfile():
  """Finish a logfile and detatch logging."""
  global logfile
  logfile = None

def setup_logfile(logname, serial):
  """Open a logfile for this device."""
  global logfile
  if logfile:
    logfile.flush()
    logfile.close()

  filename = "%s_%s_%s.log" % (logname, serial, time.time())
  logfile = open_logfile(filename)

def setup_tester_logfile(testerlogname):
  """Open a logfile for this test session."""
  global testerlogfile

  filename = '%s_%s.log' % (testerlogname, time.time())
  testerlogfile = open_logfile(filename)


def log(output):
  """Print output to console, and any open logfiles."""
  # global logfile
  # global testerlogfile
  print output
  if logfile:
    logfile.write(output)
    logfile.write('\n')
    logfile.flush()
  if testerlogfile:
    testerlogfile.write(output)
    testerlogfile.write('\n')
    testerlogfile.flush()


def check_usb(vidpid):
  """Check if vidpid is present on the system's USB."""
  if subprocess.call('lsusb -d %s > /dev/null' % vidpid, shell=True):
    return False
  return True

def check_usb_sn(vidpid):
  """Return the serial number

  Return the serial number of the first USB device with VID:PID vidpid,
  or None if no device is found.

  This will not work well with two of the same device attached.
  """
  sn = None
  try:
    lsusbstr = subprocess.check_output('lsusb -d %s -v | '
                                       'grep iSerial' % vidpid, shell=True)
    sn = lsusbstr.split()[2]
  except Exception:
    pass

  return sn

def wait_for_usb_remove(vidpid, timeout=None):
  """Wait for USB device with vidpid to be removed."""
  wait_for_usb(vidpid, timeout=timeout, desiredpresence=False)

def wait_for_usb(vidpid, timeout=None, desiredpresence=True):
  """Wait for usb device with vidpid to be present/absent."""
  if timeout:
    finish = time.time() + timeout
  while check_usb(vidpid) != desiredpresence:
    time.sleep(.1)
    if timeout:
      if time.time() > finish:
        raise TinyServoError('Timeout', 'Timeout waiting for USB %s' % vidpid)

def do_serialno(serialno, pty):
  """Set serialnumber 'serialno' via ec console 'pty'.

  Commands are:
  # > serialno set 1234
  # Saving serial number
  # Serial number: 1234
  """
  cmd = 'serialno set %s' % serialno
  regex = 'Serial number: (.*)$'

  results = pty._issue_cmd_get_results(cmd, [regex])[0]
  sn = results[1].strip().strip('\n\r')

  if sn == serialno:
    log('Success !')
    log('Serial set to %s' % sn)
  else:
    log('Serial number set to %s but saved as %s.' % (serialno, sn))
    raise TinyServoError(
        'Serial Number',
        'Serial number set to %s but saved as %s.' % (serialno, sn))

def setup_tinyservod(vidpid, interface, serialno=None):
  """Set up a pty

  Set up a pty to the ec console in order
  to send commands. Returns a pty_driver object.
  """
  vidstr, pidstr = vidpid.split(':')
  vid = int(vidstr, 16)
  pid = int(pidstr, 16)
  suart = stm32uart.Suart(vendor=vid, product=pid,
                          interface=interface, serialname=serialno)
  suart.run()
  pty = pty_driver.ptyDriver(suart, [])

  return pty

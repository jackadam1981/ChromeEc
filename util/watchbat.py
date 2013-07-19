#!/usr/bin/env python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for watching the battery using the EC console.

Sample usage: ./watchbat.py /dev/pts/10 .2 30

TODO: Make this less hacky and create a library to do other things with the EC.
"""

import ctypes
import math
import os
import select
import StringIO
import sys
import time

def DataAvail(fd):
  """Return True if there's data ready for reading on the file descriptor."""
  avail, _, _ = select.select([fd], [], [], 0)
  return fd in avail

def Clear(fd):
  """Throw away any existing data on the file descriptor."""
  while DataAvail(fd):
    os.read(fd, 1)

def ReadTill(fd, till_ch):
  """Read until the given character arrives; return all data except till_ch."""
  sio = StringIO.StringIO()
  while True:
    ch = os.read(fd, 1)
    if ch == till_ch:
      break
    sio.write(ch)

  return sio.getvalue()

class EC_Battery(object):
  def __init__(self, port):

    self._f = open(port, "r+b")
    self._fd = self._f.fileno()

  @staticmethod
  def _ParseS16(s):
    """Parse the first (signed 16-bit) hex value at the start of a string.

    Args:
      s: The value to parse
    Returns:
      val: The value
    """
    s = s.split()[0]
    val = int(s, 0)
    val = ctypes.c_int16(val).value
    return val

  def _PollBattery(self):
    """Poll the battery and return a dictionary of results; may fail sometimes.

    See PollBattery() for details.

    Returns:
      result: A dictionary containing results; None if we should retry.
    """
    Clear(self._fd)
    os.write(self._fd, "battery\n")

    ec_result = ReadTill(self._fd, ">")

    # If there's a '+' character in the result then the EC probably dropped
    # characters.  Let's just retry since I don't really trust results then.
    if '+' in ec_result:
      return None

    data_lines = (x for x in ec_result.splitlines() if ':' in x)
    partitioned_data = (x.partition(':') for x in data_lines)
    raw_dict = dict((k.strip(), v.strip()) for k, _, v in partitioned_data)

    try:
      result = {}
      result['current'] = self._ParseS16(raw_dict['I']) / 1000.
      result['discharging'] = raw_dict['I'].endswith('(DISCHG)')
      result['voltage'] = self._ParseS16(raw_dict['V']) / 1000.
      result['power'] = result['current'] * result['voltage']

      result['remaining'] = int(raw_dict['Remaining'].split()[0]) / 1000.
      result['charge'] = int(raw_dict['Charge'].split()[0])

      # TODO: figure out how to translate or get it back in mAh mode
      if raw_dict['Remaining'].endswith('mW'):
        print "Error: battery reporting mW, not mAh"
        result = None
    except KeyError:
      os.write(self._fd, "i2cscan\n")
      Clear(self._fd)
      result = None

    return result

  def PollBattery(self):
    """Poll the battery and return a dictionary of results.

    This sends the 'battery' command to the EC and then tries to parse the
    result.  It may retry if EC communication had an error.

    Returns:
      result: A dictionary containing results.  Keys:
        charge: Percentage of charge left.  0 - 100
        current: Amps of current flowing to the battery: negative for discharge.
        discharging: Should be True if current is negative.
        power: Power going into the battery.  current * voltage.
        remaining: AmpHours left in the battery.
        voltage: Volts of the battery.
    Raises:
      RuntimeError: if we failed.
    """
    tries = 0
    while True:
      tries += 1

      result = self._PollBattery()
      if result is not None:
        return result

      if tries > 5:
        raise RuntimeError("Failed to communicate with EC")

def PrintResult(prefix, result):
  if 'remaining' in result:
    print "%s%6.3fA %6.3fV %6.3fW (%.3f Ah left)" % (
      prefix, result['current'], result['voltage'], result['power'],
      result['remaining']
    )
  else:
    print "%s%6.3fA %6.3fV %6.3fW" % (
      prefix, result['current'], result['voltage'], result['power'],
    )

  sys.stdout.flush()

def _Average_Std(nums):
  """Return the average and sttdev of a list of numbers."""
  nums = list(nums)
  mean = sum(nums) / float(len(nums))
  sum_x2 = sum(x * x for x in nums)

  # TODO: I think this is right for stddev?
  stdev = math.sqrt((sum_x2 / len(nums)) - (mean * mean))

  return mean, stdev


def main(port, poll_every, how_long, force=0):
  print "Opening serial port: %s" % port

  poll_every = float(poll_every)
  how_long = float(how_long)
  force = int(force)

  if poll_every < 1:
    print "WARNING: Less than 1 second polling is probably not useful"

  bat = EC_Battery(port)

  initial = bat.PollBattery()

  if not force and not initial['discharging']:
    print "Please unplug charger and try again."
    return

  if not force and initial['charge'] < 90:
    print "Please charge your battery to > 90%% before starting (at %d%%)." % (
      initial['charge']
    )
    return

  start_time = time.time()
  this_time = start_time
  results = []
  while True:
    result = bat.PollBattery()
    PrintResult("% 7.2fs: " % (this_time - start_time), result)
    results.append(result)
    this_time += poll_every
    if this_time > (start_time + how_long):
      this_time -= poll_every
      break

    sleep_time = this_time - time.time()
    if sleep_time < 0:
      print "ERROR: can't poll that fast!"
      this_time = time.time()
      sleep_time = 0

    time.sleep(sleep_time)

  print

  average = {}
  stdev = {}
  for k in ('current', 'voltage', 'power'):
    average[k], stdev[k] = _Average_Std(x[k] for x in results)
  PrintResult(" Average: ", average)
  PrintResult(" Std dev: ", stdev)

  amp_hours_used = results[0]['remaining'] - results[-1]['remaining']
  if amp_hours_used > .001:
    hours_passed = (this_time - start_time) / (60. * 60.)
    amps_used = amp_hours_used / hours_passed
    power_used = amps_used * average['voltage']

    # Try to represent rounding errors
    # ...any value may be up to .0005 off, so first and last together means
    # our difference couldbe .001 off.
    amps_used_low = (amp_hours_used - .001) / hours_passed
    power_used_low = amps_used_low * average['voltage']
    amps_used_high = (amp_hours_used + .001) / hours_passed
    power_used_high = amps_used_high * average['voltage']

    print "Gas gauge reports:"
    print "  ~%.3fAh used (%.3f - %.3f)" % (
      amp_hours_used, amp_hours_used - .001, amp_hours_used + .001
    )
    print "  ~%.3fW average (%.3f - %.3f)" % (
      power_used, power_used_low, power_used_high
    )
  else:
    print "Test too short to print gas gauge results"


if __name__ == '__main__':
  # TODO: option parse!
  main(*sys.argv[1:])

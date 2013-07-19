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

  def PollBattery(self):
    """Poll the battery."""
    Clear(self._fd)
    os.write(self._fd, "battery\n")

    ec_result = ReadTill(self._fd, ">")

    data_lines = (x for x in ec_result.splitlines() if ':' in x)
    partitioned_data = (x.partition(':') for x in data_lines)
    raw_dict = dict((k.strip(), v.strip()) for k, _, v in partitioned_data)

    result = {}
    result['current'] = self._ParseS16(raw_dict['I']) / 1000.
    result['discharging'] = raw_dict['I'].endswith('(DISCHG)')
    result['voltage'] = self._ParseS16(raw_dict['V']) / 1000.
    result['power'] = result['current'] * result['voltage']
    result['remaining'] = int(raw_dict['Remaining'].split()[0]) / 1000.

    return result

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

def _Average_Std(nums):
  """Return the average and sttdev of a list of numbers."""
  nums = list(nums)
  mean = sum(nums) / float(len(nums))
  sum_x2 = sum(x * x for x in nums)

  # TODO: I think this is right for stddev?
  stdev = math.sqrt((sum_x2 / len(nums)) - (mean * mean))

  return mean, stdev


def main(port, poll_every, how_long):
  print "Opening serial port: %s" % port

  poll_every = float(poll_every)
  how_long = float(how_long)

  bat = EC_Battery(port)

  if not bat.PollBattery()['discharging']:
    print "Please unplug charger and try again"
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
  if amp_hours_used > .003:
    hours_passed = (this_time - start_time) / (60. * 60.)
    amps_used = amp_hours_used / hours_passed
    power_used = amps_used * average['voltage']

    print "Gas gauge reports:"
    print "  %.3f Ah used" % amp_hours_used
    print "  %.3f W average" % power_used
  else:
    print "Test too short to print gas gauge results"


if __name__ == '__main__':
  # TODO: option parse!
  main(*sys.argv[1:])

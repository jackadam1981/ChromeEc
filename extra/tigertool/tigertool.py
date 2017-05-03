#!/usr/bin/python2.7
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to control tigertail USB-C Mux board.

"""

import argparse
import re
import time

import ecusb.tiny_servo_common as c

STM_VIDPID = "18d1:5027"
serialno = "Uninitialized"

RE_SERIALNO = re.compile('^S[MN](C[PDQ][0-9]{5}|N[PDQ][0-9]{5})$')


def do_mux(mux, pty):
  """Set mux via ec console 'pty'.

  Commands are:
  # > mux A
  # TYPE-C mux is A
  """
  validmux = ["A", "B", "off"]
  if mux not in validmux:
    c.log("Mux setting %s invalid, try one of %s" % (mux, validmux))
    return False

  cmd = "\r\nmux %s\r\n" % mux
  regex = "TYPE\-C mux is ([^\s\r\n]*)\r"

  results = pty._issue_cmd_get_results(cmd, [regex])[0]
  result = results[1].strip().strip('\n\r')

  if result != mux:
    c.log("Mux set to %s but saved as %s." % (mux, result))
    #raise Exception("Mux",
    #    "Mux set to %s but saved as %s." % (mux, result))
    return False
  c.log("Mux set to %s" % result)
  return True

def do_reboot(pty):
  """Reboot via ec console pty

  Command is: reboot.
  """
  cmd = "\r\nreboot\r\n"
  regex = "Rebooting"

  try:
    results = pty._issue_cmd_get_results(cmd, [regex])[0]
    time.sleep(1)
  except Exception as e:
    print e
    return False

  return True

def do_sysjump(region, pty):
  """Set region via ec console 'pty'.

  Commands are:
  # > sysjump rw
  """
  validregion = ["ro", "rw"]
  if region not in validregion:
    c.log("Region setting %s invalid, try one of %s" % (
        region, validregion))
    return False

  cmd = "\r\nsysjump %s\r\n" % region
  try:
    pty._issue_cmd(cmd)
    time.sleep(1)
  except Exception as e:
    print e
    return False

  c.log("Region requested %s" % region)
  return True

def main():
  parser = argparse.ArgumentParser(
      description="Script to control a Tigertail board")
  parser.add_argument('-s', '--serialno', type=str,
      help="serial number of board to use", default=None)
  parser.add_argument('--setserialno', type=str,
      help="serial number to set on the board.", default=None)
  parser.add_argument('-m', '--mux', type=str,
      help="mux selection", default=None)
  parser.add_argument('-r', '--sysjump', type=str,
      help="region selection", default=None)
  parser.add_argument('--reboot', action="store_true",
      help="reboot tigertail")

  args = parser.parse_args()

  result = True

  # Let's make sure there's a tigertail
  # If nothing found in 5 seconds, fail.
  c.wait_for_usb(STM_VIDPID, 5.)

  pty = c.setup_tinyservod(STM_VIDPID, 0, serialno=args.serialno)

  if args.setserialno:
    try:
      c.do_serialno(args.setserialno, pty)
    except:
      result = False

  if args.mux:
    result &= do_mux(args.mux, pty)

  if args.sysjump:
    result &= do_sysjump(args.sysjump, pty)

  if args.reboot:
    result &= do_reboot(pty)

  if result:
    c.log("PASS")
  else:
    c.log("FAIL")
    exit(-1)



if __name__ == "__main__":
  main()

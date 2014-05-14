#!/usr/bin/env python

# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script flashes the Zinger RW firmware over the USB-PD communication
# channel using PD MCU console commands.

import array
import hashlib
import serial
import sys
import os
import subprocess
import time

PORT="--port=9990"
# RW area is half of the 32-kB flash minus the hash storage area
SIZE=16*1024 - 32

def Log(msg, screen=False):
  sys.stdout.write(msg)
  if screen:
    sys.stderr.write(msg)

def DutControlGet(key):
  cmdline = ["dut-control",PORT,key]
  output = subprocess.Popen(cmdline, stdout=subprocess.PIPE).communicate()[0]
  return output.strip().split(":")

def DutControlSet(key,val):
  return subprocess.call(["dut-control",PORT,key+":"+val])

def OpenECUart():
  port = DutControlGet("ec_uart_pty")[1]
  ser = serial.Serial(port, timeout=1)
  return ser

def SerialFlush(s):
  for l in s:
    Log(l)

def SerialExpect(s, val, timeout = 5):
  done = False
  deadline = time.time() + timeout
  while not done and (time.time() < deadline):
    for l in s:
      if val in l:
        done = True
        Log(l)
        break
      Log(l)
  if not done:
    Log("FAIL: \"%s\" missing\n" % val, screen=True)
  return done

def FlashCommand(s, cmd):
  s.write("pd flash %s\n" % cmd)
  SerialExpect(s, "DONE")

def FlashPD(filename):
  try:
    fw = file(filename).read()
  except IOError as e:
    Log("Invalid firmware %s : %s\n" % (filename, e), screen=True)
    sys.exit(2)
  # check firmware max size
  if (len(fw) > SIZE):
    Log("Firmware too large %d/%d\n" % (len(fw), SIZE), screen=True)
    sys.exit(3)
  # Compute SHA-1 hash for the full (padded) RW firmware
  padded_fw = fw + '\xff'*(SIZE - len(fw))
  sha = hashlib.sha1(padded_fw).digest()
  sha_str = " ".join(["%08x" % (w) for w in array.array("I", sha)])

  # pad the firmware to a multiple of 6 U32
  if (len(fw) % 24):
    fw += '\xff'*(24 - len(fw) % 24)
  words = array.array("I", fw)

  try:
    ec = OpenECUart()
  except OSError as e:
    Log("Cannot open EC pty : %s\n" % (e), screen=True)
    sys.exit(3)

  Log("Flashing %d bytes\n" % (len(fw)), screen=True)
  # reset flashed hash to reboot in RO
  FlashCommand(ec,"hash" + " 00000000"*5)
  # reboot in RO
  FlashCommand(ec,"reboot")
  # erase all RW partition
  FlashCommand(ec,"erase")
  # write firmware content
  for i in xrange(len(words) / 6):
    chunk = words[i * 6: (i + 1)*6]
    FlashCommand(ec," ".join(["%08x" % (w) for w in chunk]))
  # write new firmware hash
  FlashCommand(ec,"hash " + sha_str)
  # reboot in RW
  FlashCommand(ec,"reboot")

  Log("flashing DONE.\n", screen=True)
  SerialFlush(ec)
  ec.close()
  Log("SHA-1: %s\n" % (sha_str), screen=True)

if __name__=="__main__":
  if len(sys.argv) < 2:
    Log("Syntax: %s firmware.bin\n" %(sys.argv[0]), screen=True)
    sys.exit(1)
  FlashPD(sys.argv[1])

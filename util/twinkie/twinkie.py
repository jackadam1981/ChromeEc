#!/usr/bin/env python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""USB interface with the Twinkie dongle."""

from __future__ import print_function

import errno
import usb
from utils import Log

TWINKIE_VID = 0x18d1
TWINKIE_PID = 0x500a

EP_CON_TX = 0x01
EP_CON_RX = 0x81
EP_SNIFFER = 0x83

INTERFACE_CON = 0
INTERFACE_SNIFFER = 1

PROMPT = '>'

class Twinkie(object):
  """Interface to a Twinkie dongle through a USB connection."""
  def __init__(self, log=None):
    self.log = log if log else Log()
    self._valid = False
    self._dev = self.find()
    if not self._dev:
      return
    self._hnd = self._dev.open()
    res = self.tryClaimInterface(INTERFACE_CON, self._hnd)
    if not res:
      return
    self._hnd_sniff = self._dev.open()
    res = self.tryClaimInterface(INTERFACE_SNIFFER, self._hnd_sniff)
    if not res:
      return
    self._valid = True

  def find(self):
    busses = usb.busses()
    for bus in busses:
      devices = bus.devices
      for dev in devices:
        if dev.idVendor == TWINKIE_VID and dev.idProduct == TWINKIE_PID:
          self.log.Debug("Twinkie FOUND")
          return dev
    return None

  def tryClaimInterface(self, iface, hnd):
    try:
      hnd.detachKernelDriver(iface)
    except usb.core.USBError, e:
      if e.errno == errno.ENOENT:
        pass
    try:
      hnd.claimInterface(iface)
    except usb.core.USBError, e:
      self.log.Error("Failed to get Twinkie interface %d\n" % (iface))
      return False
    return True

  def isValid(self):
    return self._valid

  def readCon(self, size=64):
    try:
      val = self._hnd.bulkRead(EP_CON_RX, size)
    except usb.core.USBError:
      val = ""
    return val

  def writeCon(self, text):
    return self._hnd.bulkWrite(EP_CON_TX, text)

  def readSniffer(self, size=64):
    return self._hnd_sniff.bulkRead(EP_SNIFFER, size)

  def sendCommand(self, cmd, waitAnswer=True, prompt=PROMPT):
    cmdStr = cmd+"\r\n"
    self.log.Debug("CMD:%s" % (cmdStr.strip()))
    self.writeCon(cmdStr)
    output = ""
    if waitAnswer:
      while True:
        pkt = self.readCon()
        if not pkt:
          continue
        line = pkt.tostring()
        if prompt in line:
          output += line[:line.index(prompt)]
          break
        output += line

    return output[len(cmdStr):].strip()

  def writeWords(self, idx, words):
    params = " ".join(["%08x" % (w) for w in words])
    self.sendCommand("tw bufwr %d %s" % (idx, params))

  def writeFsmBuffer(self, words):
    cnt4 = len(words)/4
    for i in xrange(cnt4):
      self.writeWords(i * 4, words[i*4:(i+1)*4])
    if len(words) % 4:
      self.writeWords(cnt4 * 4, words[cnt4*4:])

  def read2U16(self, idx):
    val = self.sendCommand("tw bufrd %d" % (idx))
    self.log.Debug("RD/%d = %s" %(idx, val))
    u32 = int(val, 16)
    return (u32 & 0xFFFF, u32 >> 16)

  def runFsm(self):
    return self.sendCommand("tw fsm 0")



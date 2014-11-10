#!/usr/bin/env python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from injector import CmdFsm, INJ_POL_CC1, INJ_POL_CC2, INJ_POL_AUTO
from twinkie import Twinkie
from utils import Log

class TestSequence(CmdFsm):
  def __init__(self, twinkie, log=None):
    super(TestSequence, self).__init__()
    self._log = log if log else twinkie._log
    self._tw = twinkie
    self._values = {}
    self._success = True

  def sequence(self):
    # To be overloaded by Test Classes
    pass

  def check(self, values):
    # To be overloaded by Test Classes
    pass

  def run(self):
    # Generate the buffer of commands
    self.sequence()
    # Ensure the sequence has a proper ending
    self.end()
    # Send the FSM content
    self._tw.writeFsmBuffer(self._words)
    # Start executing the FSM
    res = self._tw.runFsm()
    self._log.Debug("FSM result %s / %d" % (res, len(self._words)))
    # Retrieve values read during execution
    for g in self._gets.iterkeys():
      lo,hi = self._tw.read2U16(self._gets[g][0])
      self._values[g] = lo, hi, "%d %s %d %s" % (lo, self._gets[g][1][0], hi, self._gets[g][1][1])
    self._log.Debug(self._values)
    # Run test assertions
    self.check(self._values)
    if self._success:
      self._log.Info("PASS")

  def fail(self, message):
    self._log.Info("FAIL: %s" % (message))
    self._success = False

class SinkConnect(TestSequence):
  def sequence(self):
    # VBus should be off when disconnected
    self.getVBus('VBUS_cold')
    # Source with captive cable : only one CC connected
    self.setResistors(CC1='RD', CC2='RD')
    # Wait 500ms for vSafe5V to appear
    self.wait(500)
    # Check vSafe5V
    self.getVBus('VBUS_vSafe5V')
    # Disconnect
    self.setResistors(CC1='none', CC2='none')

  def check(self,values):
    cold = values['VBUS_cold'][0]
    safe5v = values['VBUS_vSafe5V'][0]
    self._log.Debug("VBUS cold %d mV vSafe5V %d" % (cold, safe5v))
    if cold > 200:
      self.fail("VBUS not off when unplugged : %d mV" % (cold))
    if safe5v < 4500 or safe5v > 5500:
      self.fail("VBUS vSafe5V out of range : %d mV" % (safe5v))

class SinkPing(TestSequence):
  def sequence(self):
    # Source with captive cable : only one CC connected
    self.setResistors(CC1='RD', CC2='RD')
    # Wait 2s for vSafe5V to appear and things to stabilize
    self.wait(2000)
    # Guess the source polarity
    self.setPolarity(INJ_POL_AUTO)
    # Send a ping message
    self.send(0x0045)
    # Wait for the GoodCRC
    self.wait(5000, minEdges=230)
    # Disconnect
    self.setResistors(CC1='none', CC2='none')

class SinkSrcCap(TestSequence):
  def sequence(self):
    # Source with captive cable : only one CC connected
    self.setResistors(CC1='RD', CC2='RD')
    # Wait 2s for vSafe5V to appear and things to stabilize
    self.wait(2000)
    # Guess the source polarity
    self.setPolarity(INJ_POL_AUTO)
    # Send a GET_SOURCE_CAP message
    self.send(0x0047)
    # Wait for the GoodCRC
    self.wait(5000, minEdges=230)
    # Wait for the SRC_CAP
    self.wait(5000, minEdges=280)
    # Send a GOOD_CRC for SRC_CAP
    self.send(0x0041)
    # Disconnect
    self.setResistors(CC1='none', CC2='none')

if __name__=='__main__':
  log = Log()
  tw = Twinkie(consoleOnly=True, log=log)
  # Test a source by behaving as a not-PD-enabled Sink
  SinkConnect(tw).run()
  # Send a Ping packet
  SinkPing(tw).run()
  # Test Source capabilities
  SinkSrcCap(tw).run()

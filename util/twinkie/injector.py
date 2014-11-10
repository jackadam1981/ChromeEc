#!/usr/bin/env python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Automate the generation of events injected in the test sequence."""

from __future__ import print_function

### FSM constants ####
# FSM Commands
INJ_CMD_END = 0x0
INJ_CMD_SEND = 0x1
INJ_CMD_WAVE = 0x2
INJ_CMD_HRST = 0x3
INJ_CMD_WAIT = 0x4
INJ_CMD_GET = 0x5
INJ_CMD_SET = 0x6
INJ_CMD_JUMP = 0x8
INJ_CMD_EXPCT = 0xC
INJ_CMD_NOP = 0xF
# INJ_CMD_SET indexes
INJ_SET_RESISTOR1 = 0
INJ_SET_RESISTOR2 = 1
INJ_SET_RECORD = 2
INJ_SET_TX_SPEED = 3
INJ_SET_RX_THRESH = 4
INJ_SET_POLARITY = 5
INJ_SET_TRACE = 6
# INJ_CMD_SET indexes
INJ_GET_CC = 0
INJ_GET_VBUS = 1
INJ_GET_VCONN = 2
INJ_GET_POLARITY = 3
# INJ_SET_RESISTORx index
INJ_RES_NONE = 0
INJ_RES_RA = 1
INJ_RES_RD = 2
INJ_RES_RPUSB = 3
INJ_RES_RP1A5 = 4
INJ_RES_RP3A0 = 5
RESISTORS = {"none":INJ_RES_NONE,
             "RA":INJ_RES_RA,
             "RD":INJ_RES_RD,
             "RPUSB":INJ_RES_RPUSB,
             "RP1A5":INJ_RES_RP1A5,
             "RP3A0":INJ_RES_RP3A0}

INJ_POL_CC1 = 0
INJ_POL_CC2 = 1
INJ_POL_AUTO = 0xFFFF

TRACE_MODE_OFF = 0
TRACE_MODE_RAW = 1
TRACE_MODE_ON  = 2

class CmdFsm(object):
  """Create the Finite State Machine program to run on a Twinkie."""
  def __init__(self):
    self._words = []
    self._gets = {}

  def nextIndex(self):
    return len(self._words)

  def word(self, cmdi, arg=None, arg0=None, arg1=None, arg2=None, arg12=None):
    if arg12:
      arg1 = arg12 & 0xff
      arg2 = (arg12 >> 8) & 0xF
    if not arg0:
      arg0 = 0
    if not arg1:
      arg1 = 0
    if not arg2:
      arg2 = 0
    if not arg:
      arg = (arg0 & 0xFFFF) | ((arg1 & 0xFF) << 16) | ((arg2 & 0xF) << 24)
    self._words.append((cmdi << 28) | (arg & 0x0FFFFFFF))
    return self.nextIndex() - 1

  def data(self, words):
    self._words.extend(words)
    return self.nextIndex() - 1

  def end(self):
    self.word(INJ_CMD_END)

  def nop(self):
    self.word(INJ_CMD_NOP)

  def jump(self, index):
    self.word(cmdi=INJ_CMD_JUMP, arg0=index)

  def getParam(self, get_index, name=None, units=("lo", "hi")):
    # Store the result in the instruction slot
    store_index = self.nextIndex()
    self.word(cmdi=INJ_CMD_GET, arg0=store_index, arg1=get_index)
    # Record that we have something to retrieve after execution
    if not name:
      name = "GET%d@%d" % (get_index, store_index)
    self._gets[name] = (store_index, units)

  def setParam(self, set_index, value):
    self.word(cmdi=INJ_CMD_SET, arg0=value, arg1=set_index)

  def send(self, header, payload=None):
    if not payload:
      payload = []
    # SEND , JUMP, PAYLOAD WORDS , next instr
    payload_idx = self.nextIndex() + 2 if len(payload) else 0
    self.word(INJ_CMD_SEND, arg0=header, arg1=payload_idx, arg2=len(payload))
    if payload_idx:
      self.word(INJ_CMD_JUMP, arg0=payload_idx + len(payload))
      self.data(payload)

  def wave(self, bitLen, waveform=None):
    if not waveform:
      waveform = []
    # WAVE , JUMP, waveform WORDS , next instr
    waveform_idx = self.nextIndex() + 2 if len(waveform) else 0
    self.word(INJ_CMD_WAVE, arg0=bitLen, arg1=waveform_idx)
    self.word(INJ_CMD_JUMP, arg0=waveform_idx + len(waveform))
    self.data(waveform)

  def wait(self, timeoutMs, minEdges=0):
    self.word(INJ_CMD_WAIT, arg0=timeoutMs, arg12=minEdges)

  def expect(self, timeoutMs, cmd=0):
    self.word(INJ_CMD_EXPECT, arg0=timeoutMs, arg2=cmd)

  def hardReset(self):
    self.word(INJ_CMD_HRST)

  def setRecord(self, mask):
    self.setParam(INJ_SET_RECORD, mask)

  def setTxSpeed(self, speedHz):
    self.setParam(INJ_SET_TX_SPEED, speedHz)

  def setRxThreshold(self, thresholdmV):
    self.setParam(INJ_SET_RX_THRESH, thresholdmV)

  def setResistors(self, CC1=None, CC2=None):
    res1 = INJ_RES_NONE
    res2 = INJ_RES_NONE
    if CC1 and CC1 in RESISTORS:
      res1 = RESISTORS[CC1]
    if CC2 and CC2 in RESISTORS:
      res2 = RESISTORS[CC2]
    self.setParam(INJ_SET_RESISTOR1, res1)
    self.setParam(INJ_SET_RESISTOR2, res2)

  def setPolarity(self, polarity):
    self.setParam(INJ_SET_POLARITY, polarity)

  def setTrace(self, trace):
    self.setParam(INJ_SET_TRACE, trace)

  def getCC(self, name='CC'):
    self.getParam(INJ_GET_CC, name=name, units=("mV", "mV"))

  def getVBus(self, name='VBus'):
    self.getParam(INJ_GET_VBUS, name=name, units=("mV", "mA"))

  def getVConn(self, name='VConn'):
    self.getParam(INJ_GET_VCONN, name=name, units=("mV", "mA"))

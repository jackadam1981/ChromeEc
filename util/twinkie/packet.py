#!/usr/bin/env python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""USB Power Delivery packet builder."""

from __future__ import print_function

import struct
import zlib   # for crc32

# K-codes for special symbols
PD_SYNC1 = 0x18
PD_SYNC2 = 0x11
PD_SYNC3 = 0x06
PD_RST1  = 0x07
PD_RST2  = 0x19
PD_EOP   = 0x0D

# Start of Packet sequence : three Sync-1 K-codes, then one Sync-2 K-code
PD_SOP = PD_SYNC1 | (PD_SYNC1<<5) | (PD_SYNC1<<10) | (PD_SYNC2<<15)
PD_SOP_PRIME = PD_SYNC1 | (PD_SYNC1<<5) | (PD_SYNC3<<10) | (PD_SYNC3<<15)
PD_SOP_PRIME_PRIME = PD_SYNC1 | (PD_SYNC3<<5) | (PD_SYNC1<<10) | (PD_SYNC3<<15)
PD_SOP_PRIME_DBG = PD_SYNC1 | (PD_RST2<<5) | (PD_RST2<<10) | (PD_SYNC3<<15)
PD_SOP_PRIME_PRIME_DBG = PD_SYNC1 | (PD_RST2<<5) | (PD_SYNC3<<10) | (PD_SYNC2<<15)

# Hard Reset sequence : three RST-1 K-codes, then one RST-2 K-code
PD_HARD_RESET = PD_RST1 | (PD_RST1 << 5) | (PD_RST1 << 10) | (PD_RST2 << 15)
# Cable Reset sequence : RST-1/SYNC-1/RST-1/SYNC-3
PD_CABLE_RESET = PD_RST1 | (PD_SYNC1 << 5) | (PD_RST1 << 10) | (PD_SYNC3 << 15)

# Control Message type
# 0 Reserved
PD_CTRL_GOOD_CRC = 1
PD_CTRL_GOTO_MIN = 2
PD_CTRL_ACCEPT = 3
PD_CTRL_REJECT = 4
PD_CTRL_PING = 5
PD_CTRL_PS_RDY = 6
PD_CTRL_GET_SOURCE_CAP = 7
PD_CTRL_GET_SINK_CAP = 8
PD_CTRL_DR_SWAP = 9
PD_CTRL_PR_SWAP = 10
PD_CTRL_VCONN_SWAP = 11
PD_CTRL_WAIT = 12
PD_CTRL_SOFT_RESET = 13
# 14-15 Reserved 

# Data message type
# 0 Reserved
PD_DATA_SOURCE_CAP = 1
PD_DATA_REQUEST = 2
PD_DATA_BIST = 3
PD_DATA_SINK_CAP = 4
PD_DATA_BAT_STATUS = 5
PD_DATA_ALERT = 6
# 7-14 Reserved
PD_DATA_VENDOR_DEF = 15

# Protocol revision
PD_REV10 = 0
PD_REV20 = 1
PD_REV30 = 2

# Power role
PD_ROLE_SINK = 0
PD_ROLE_SOURCE = 1
# Data role
PD_ROLE_UFP = 0
PD_ROLE_DFP = 1

class Packet(object):
  """Represents a USB PD packet."""
  def __init__(self, cmd=PD_CTRL_PING, sink=True, answerPacket=None, prevPacket=None):
    self._payload = []
    self._rev = PD_REV20
    self._cmd = cmd
    if answerPacket:
      self._drole = prevPacket.getDataRole() ^ 1
      self._prole = prevPacket.getPowerRole() ^ 1
      self._seq = answerPacket.getMessageId()
    elif prevPacket:
      self._drole = prevPacket.getDataRole()
      self._prole = prevPacket.getPowerRole()
      self._seq = (prevPacket.getMessageId() + 1) & 7
    else:
      self._drole = PD_ROLE_UFP if sink else PD_ROLE_DFP
      self._prole = PD_ROLE_SINK if sink else PD_ROLE_SOURCE
      self._seq = 0

  def header(self):
    return self._cmd | (self._rev << 6) | (self._drole << 5) | (self._prole << 8) | (self._seq << 9) | (len(self._payload) << 12)

  def getDataRole(self):
    return self._drole

  def getPowerRole(self):
    return self._prole

  def getMessageId(self):
    return self._seq

  def getInjector(self):
    header = "0x%04x" % (self.header())
    words = " ".join([ "%08x" % (w) for w in self._payload ])
    return "%s %s" % (header, words) 

  ENC4B5B = [
    0x1E, # 0 = 0000 => 11110
    0x09, # 1 = 0001 => 01001
    0x14, # 2 = 0010 => 10100
    0x15, # 3 = 0011 => 10101
    0x0A, # 4 = 0100 => 01010
    0x0B, # 5 = 0101 => 01011
    0x0E, # 6 = 0110 => 01110
    0x0F, # 7 = 0111 => 01111
    0x12, # 8 = 1000 => 10010
    0x13, # 9 = 1001 => 10011
    0x16, # A = 1010 => 10110
    0x17, # B = 1011 => 10111
    0x1A, # C = 1100 => 11010
    0x1B, # D = 1101 => 11011
    0x1C, # E = 1110 => 11100
    0x1D, # F = 1111 => 11101
  ]

  def encode4b5b(self):
    ### TBD ###
    return []

  def computeCrc32(self):
    head = self.header()
    data = self._payload
    bdata = struct.pack('<H'+'I'*len(data), head & 0xffff,
                        *tuple([d & 0xffffffff for d in data]))
    return zlib.crc32(bdata)

  def getRawWaveForm(self):
    ### TBD ###
    head = self.header()
    data = self._payload
    bdata = struct.pack('<H'+'I'*len(self._payload), head & 0xffff,
                        *tuple([d & 0xffffffff for d in data]))
    ####
    preamble = []
    sop = []
    header = [] # 4B/5B
    payload = [] # 4B/5B
    crc = [] # 4B/5B
    eop = []
    # BMC it
    return []

class Ping(Packet):
  def __init__(self, prevPacket=None):
    super(Ping,self).__init__(cmd=PD_CTRL_PING, prevPacket=prevPacket)

class GetSourceCap(Packet):
  def __init__(self, prevPacket=None):
    super(GetSourceCap,self).__init__(cmd=PD_CTRL_GET_SOURCE_CAP, prevPacket=prevPacket)

if __name__ == '__main__':
  print("PING 0x%04x" % Ping().header())
  print("GET_SOURCE_CAP 0x%04x" % GetSourceCap().header())

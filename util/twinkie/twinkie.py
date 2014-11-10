#!/usr/bin/env python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import array
import errno
import sys
import usb

TWINKIE_VID=0x18d1
TWINKIE_PID=0x500a

EP_CON_TX=0x01
EP_CON_RX=0x81
EP_SNIFFER=0x83

INTERFACE_CON=0
INTERFACE_SNIFFER=1

PROMPT='>'

class Twinkie:
	def __init__(self, consoleOnly=False):
		self._valid = False
		self._dev = self.find()
		if not self._dev:
			return
		self._hnd = self._dev.open()
		res = self.tryClaimInterface(INTERFACE_CON)
		if not res:
			return
		if not consoleOnly:
			res = self.tryClaimInterface(INTERFACE_SNIFFER)
			if not res:
				return
		self._valid = True

	def find(self):
		busses = usb.busses()
		for bus in busses:
			devices = bus.devices
			for dev in devices:
				if dev.idVendor == TWINKIE_VID and dev.idProduct == TWINKIE_PID:
					print "Twinkie FOUND"
					return dev
		return None

	def tryClaimInterface(self, iface):
		try:
	            	self._hnd.detachKernelDriver(iface)
		except usb.core.USBError, e:
			if e.errno == errno.ENOENT:
				pass
		try:
			self._hnd.claimInterface(iface)
		except usb.core.USBError, e:
			sys.stderr.write("Failed to get Twinkie interface %d\n" % (iface))
			return False
		return True

	def isValid(self):
		return self._valid

	def readCon(self, size=64):
		return self._hnd.bulkRead(EP_CON_RX, size)

	def writeCon(self, text):
		return self._hnd.bulkWrite(EP_CON_TX, text)

	def readSniffer(self, size=10*1024):
		return self._hnd.bulkRead(EP_SNIFFER, size)

	def sendCommand(self, cmd, waitAnswer=True):
		cmdStr = cmd+"\r\n"
		self.writeCon(cmdStr)
		output = ""
		if waitAnswer:
			while True:
				pkt = self.readCon()
				if not pkt:
					continue
				line = pkt.tostring()
				if PROMPT in line:
					output += line[:line.index(PROMPT)]
					break
				output += line

		return output[len(cmdStr):].strip()

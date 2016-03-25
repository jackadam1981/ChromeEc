#!/usr/bin/python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Test program to access I2C via stm32 usb interface.
import array
import struct
import sys
import time

import usb



"""Accesses power logging through stm32 usb endpoint."""

class Spower(object):
  """Power class to access devices on the bus.

  Usage:
    bus = Spower()

  Instance Variables:
    _logger: Sgpio tagged log output
    _dev: pyUSB device object
    _read_ep: pyUSB read endpoint for this interface
    _write_ep: pyUSB write endpoint for this interface
  """

  INA231 = 1


  def __init__(self, vendor=0x18d1,
               product=0x501a, interface=7, serialname=None ):
    # Find the stm32.
    dev = usb.core.find(idVendor=vendor, idProduct=product)
    #dev = usb.core.find(idVendor=vendor)
    if dev is None:
      raise Exception("Power", "USB device not found")

    print "Found stm32: %04x:%04x" % (vendor, product)
    self._dev = dev

    # Get an endpoint instance.
    try:
      dev.set_configuration()
    except:
      pass
    cfg = dev.get_active_configuration()

    intf = usb.util.find_descriptor(cfg, custom_match=lambda i: \
        i.bInterfaceClass==255 and i.bInterfaceSubClass==0x53)

    self._intf = intf
    print "InterfaceNumber: %s" % intf.bInterfaceNumber

    read_ep = usb.util.find_descriptor(
      intf,
      # match the first IN endpoint
      custom_match = \
      lambda e: \
          usb.util.endpoint_direction(e.bEndpointAddress) == \
          usb.util.ENDPOINT_IN
    )

    self._read_ep = read_ep
    print "Reader endpoint: 0x%x" % read_ep.bEndpointAddress

    write_ep = usb.util.find_descriptor(
      intf,
      # match the first OUT endpoint
      custom_match = \
      lambda e: \
          usb.util.endpoint_direction(e.bEndpointAddress) == \
          usb.util.ENDPOINT_OUT
    )

    self._write_ep = write_ep
    print "Writer endpoint: 0x%x" % write_ep.bEndpointAddress

    print "Set up stm32 power"

  def wr_command(self, write_list, read_count=1):
    """Write command to logger logic..

    This function writes byte command values list to stm, then reads
    byte status.

    Args:
      write_list: list of command byte values [0~255].
      read_count: number of status byte values to read.

    Interface:
      write: [command, data ... ]
      read: [status ]
    """
    print "Spower.wr_command(write_list=[%s] (%d), read_count=%s)" % (
          write_list, len(write_list), read_count)

    # Clean up args from python style to correct types.
    write_length = 0
    if write_list:
      write_length = len(write_list)
    if not read_count:
      read_count = 0

    # Send command to stm32.
    cmd = write_list
    ret = self._write_ep.write(cmd, 100)

    print "RET: %s " % ret

    # Read back response if necessary.
    if read_count:
      #bytesread = self._read_ep.read(read_count, 1000)
      bytesread = self._read_ep.read(64, 1000)
      print "BYTES: [%s]" % bytesread

      if len(bytesread) != read_count:
        #raise Exception("Power", "Read status failed.")
        print "Power: Read status failed."

      print "STATUS: 0x%02x" % int(bytesread[0])
      return bytesread[0]

    return None

  def reset(self):
    """Reset the power interface on the stm32"""
    cmd = struct.pack("<H", 0x0000)
    ret = self.wr_command(cmd, read_count=0)
    print "Command RESET: %s" % "success" if ret == 0 else "failure"

  def stop(self):
    """Stop any active data aquisition."""
    cmd = struct.pack("<H", 0x0001)
    ret = self.wr_command(cmd, read_count=0)
    print "Command STOP: %s" % "success" if ret == 0 else "failure"

  def start(self, integration_ms):
    """Start data aquisition.

    Args:
      integration_ms: int, how many ms between samples, and
                      how often the data block must be read.
    """
    cmd = struct.pack("<HI", 0x0003, integration_ms)
    ret = self.wr_command(cmd)
    print "Command START: %s" % "success" if ret == 0 else "failure"

  def add_ina(self, bus, ina_type, addr, extra, voltage, resistance):
    """Add an INA to the data aquisition list.

    Args:
      bus: which i2c bus the INA is on. Same ordering as Si2c.
      ina_type: which model INA. 0x1 -> INA231
      addr: 7 bit i2c addr of this INA
      extra: extra data for nonstandard configs.
      voltage: int, base voltage in mV
      resistance: int, shunt resistance in mOhm
    """
    # 0x0002, 1B: bus, 1B:INA type, 1B: INA addr, 1B: extra, 4B: voltage, 4B: Rs
    cmd = struct.pack("<HBBBBII", 0x0002, bus, ina_type, addr, extra, voltage, resistance)
    ret = self.wr_command(cmd)
    print "Command ADD_INA: %s" % "success" if ret == 0 else "failure"

  def read_line(self):
    """Read a line of data fromteh setup INAs"""
    try:
      bytesread = self._read_ep.read(64, 1000)
      #print "BYTES: [%s]" % bytesread
    except:
      #print "READ LINE FAILED"
      return

    if len(bytesread) < 2:
      print "READ LINE FAILED bytes: %d" % len(bytesread)
      return

    status, size = struct.unpack("<BB", bytesread[0:2])
    if len(bytesread) < (size * 2 + 6):
      print "READ LINE FAILED st:%d size:%d len:%d" % (status, size, len(bytesread))
      return

    timestamp = struct.unpack("<I", bytesread[2:6])[0]
    print "READ LINE: st:%d size:%d time:%d" % (status, size, timestamp)
    ftimestamp = float(timestamp) / 1000000.
    for i in range(0, size):
      idx = 6 + 2*i
      mv = struct.unpack("<H", bytesread[idx:idx+2])[0]
      fmv = float(mv) * .0025
      print "READ %d: %fs: %fmV" % (i, ftimestamp, fmv)

    #print "STATUS: 0x%02x" % int(bytesread[0])
    return bytesread[0]



def main():
  if len(sys.argv) < 1:
    print "Usage: %s " % sys.argv[0]
    return

  args = len(sys.argv)

  p = Spower()
  p.read_line()
  p.read_line()
  p.read_line()
  p.read_line()
  p.reset()
  p.add_ina(0, p.INA231, 0x40, 0, 3300, 1000)
  p.add_ina(0, p.INA231, 0x41, 0, 1800, 100)
  p.add_ina(0, p.INA231, 0x42, 0, 1800, 1000)
  p.add_ina(0, p.INA231, 0x43, 0, 1800, 1000)
  p.start(100)
  try:
    while True:
      p.read_line()
  except:
    pass
  p.stop()

if __name__ == "__main__":
  main()


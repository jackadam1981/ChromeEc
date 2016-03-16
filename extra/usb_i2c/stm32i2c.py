#!/usr/bin/python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Test program to access I2C via stm32 usb interface.
import array
import sys

import usb



"""Accesses I2C buses through stm32 usb endpoint."""

class Si2cBus(object):
  """I2C bus class to access devices on the bus.

  Usage:
    bus = Si2cBus()
    # read 1 byte from slave(0x48) register(0x16)
    bus.wr_rd(0x48, [0x16], 1)
    # write 2 bytes to slave(0x48) register(0x20)
    bus.wr_rd(0x48, [0x20, 0x01, 0x02])

  Instance Variables:
    _logger: Sgpio tagged log output
    _dev: pyUSB device object
    _read_ep: pyUSB read endpoint for this interface
    _write_ep: pyUSB write endpoint for this interface
  """

  def __init__(self, vendor=0x18d1,
               product=0x501a, interface=4, serialname=None ):
    # Find the stm32.
    #dev = usb.core.find(idVendor=vendor, idProduct=product)
    dev = usb.core.find(idVendor=vendor)
    if dev is None:
      raise Exception("I2C", "USB device not found")

    print "Found stm32: %04x:%04x" % (vendor, product)
    self._dev = dev

    # Get an endpoint instance.
    try:
      dev.set_configuration()
    except:
      pass
    cfg = dev.get_active_configuration()

    intf = usb.util.find_descriptor(cfg, custom_match=lambda i: \
        i.bInterfaceClass==255 and i.bInterfaceSubClass==0x52)

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

    print "Set up stm32 i2c"

  def wr_rd(self, slave_address, write_list, read_count=None):
    """Implements hdctools wr_rd() interface.

    This function writes byte values list to I2C device, then reads
    byte values from the same device.

    Args:
      slave_address: 7 bit I2C slave address.
      write_list: list of output byte values [0~255].
      read_count: number of byte values to read from device.

    Interface:
      write: [addr, write_count, read_count, data ... ]
      read: [data .. ]
    """
    print "Si2c.wr_rd(slave_address=0x%x, write_list=%s, read_count=%s)" % (
          slave_address, write_list, read_count)

    # Clean up args from python style to correct types.
    write_length = 0
    if write_list:
      write_length = len(write_list)
    if not read_count:
      read_count = 0

    # Send wr_rd command to stm32.
    port = 0
    cmd = [port, slave_address, write_length, read_count] + write_list
    print "WR: 0x%x %s" % (slave_address, cmd)
    ret = self._write_ep.write(cmd, 100)

    print "RET: 0x%x %s " % (slave_address, ret)

    # Read back response if necessary.
    bytesread = self._read_ep.read(read_count + 4, 1000)
    print "BYTES: 0x%x %s " % (slave_address, bytes)


    if len(bytesread) < 4:
      raise Exception("I2C", "Read status failed.")

    print "STATUS: 0x%02x%02x" % (int(bytesread[1]), int(bytesread[0]))
    return bytesread[4:]

def main():
  if len(sys.argv) < 4:
    print "Usage: %s address writevalue [writevalue writevalue] readcount" % sys.argv[0]
    return

  args = len(sys.argv)

  address = int(sys.argv[1], base=0)
  writes = []
  for i in range(2, args - 1):
    writevalue = int(sys.argv[i], base=0)
    writes.append(writevalue)

  readcount = int(sys.argv[args - 1],base=0)

  bus = Si2cBus()
  ret = bus.wr_rd(address, writes, read_count=readcount)
  print ret

if __name__ == "__main__":
  main()


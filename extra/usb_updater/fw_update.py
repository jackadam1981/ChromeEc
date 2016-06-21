#!/usr/bin/python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Test program to access I2C via stm32 usb interface.
import argparse
import array
import json
import os
import struct
import sys
import time
from pprint import pprint

import usb

debug = False
def debuglog(msg):
  if debug:
    print msg

def logoutput(msg):
  print msg
  sys.stdout.flush()

"""Accesses power logging through stm32 usb endpoint."""

class Supdate(object):
  """Power class to access devices on the bus.

  Usage:
    bus = Spower()

  Instance Variables:
    _logger: Sgpio tagged log output
    _dev: pyUSB device object
    _read_ep: pyUSB read endpoint for this interface
    _write_ep: pyUSB write endpoint for this interface
  """

  def __init__(self, vendor=0x18d1,
               product=0x501a, serialname=None ):
    # Find the stm32.
    dev_list = usb.core.find(idVendor=vendor, idProduct=product, find_all=True)
    #dev = usb.core.find(idVendor=vendor)
    if dev_list is None:
      raise Exception("Power", "USB device not found")

    # Check if we have multiple stm32s and we've specified the serial.
    dev = None
    if serialname:
      for d in dev_list:
        if usb.util.get_string(d, 256, d.iSerialNumber) == serialname:
          dev = d
          break
      if dev is None:
        raise SusbError("USB device(%s) not found" % serialname)
    else:
      #dev = dev_list.next()
      try:
        dev = dev_list[0]
      except:
        dev = dev_list.next()

    debuglog("Found stm32: %04x:%04x" % (vendor, product))
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
    debuglog("InterfaceNumber: %s" % intf.bInterfaceNumber)

    read_ep = usb.util.find_descriptor(
      intf,
      # match the first IN endpoint
      custom_match = \
      lambda e: \
          usb.util.endpoint_direction(e.bEndpointAddress) == \
          usb.util.ENDPOINT_IN
    )

    self._read_ep = read_ep
    debuglog("Reader endpoint: 0x%x" % read_ep.bEndpointAddress)

    write_ep = usb.util.find_descriptor(
      intf,
      # match the first OUT endpoint
      custom_match = \
      lambda e: \
          usb.util.endpoint_direction(e.bEndpointAddress) == \
          usb.util.ENDPOINT_OUT
    )

    self._write_ep = write_ep
    debuglog("Writer endpoint: 0x%x" % write_ep.bEndpointAddress)

    debuglog("Set up stm32 power")

  def wr_command(self, write_list, read_count=1, wtimeout=100, rtimeout=1000):
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
    debuglog("Spower.wr_command(write_list=[%s] (%d), read_count=%s)" % (
          list(bytearray(write_list)), len(write_list), read_count))

    # Clean up args from python style to correct types.
    write_length = 0
    if write_list:
      write_length = len(write_list)
    if not read_count:
      read_count = 0

    # Send command to stm32.
    if write_list:
      cmd = write_list
      ret = self._write_ep.write(cmd, wtimeout)
      debuglog("RET: %s " % ret)

    # Read back response if necessary.
    if read_count:
      #bytesread = self._read_ep.read(read_count, 1000)
      bytesread = self._read_ep.read(512, rtimeout)
      debuglog("BYTES: [%s]" % bytesread)

      if len(bytesread) != read_count:
        #raise Exception("Power", "Read status failed.")
        #print "Power: read count %d != expected %d." % (len(bytesread), read_count)
        pass

      debuglog("STATUS: 0x%02x" % int(bytesread[0]))
      if read_count == 1:
        return bytesread[0]
      else:
        return bytesread

    return None

  def stop(self):
    """Finalize system flash and exit."""
    cmd = struct.pack("<I", 0xB007AB1E)
    read = self.wr_command(cmd, read_count=4)

    if len(read) == 4:
      print "EVERYTHING IS OK"
      return

    raise Exception("Update", "Stop failed [%s]" % read)


  def write_file(self, region):
    flash_base = self._brdcfg["flash"]
    offset = self._base - flash_base
    if offset != self._brdcfg['regions'][region][0]:
      raise Exception("Update", "Region %s offset 0x%x != available offset 0x%x" % (
          region, self._brdcfg['regions'][region][0], offset))

    length = self._brdcfg['regions'][region][1]
    print "Sending"

    self._binfile.seek(offset)
    maxpacket = 32

    while length > 0:
      pagesize = min(length, 128)
      print "cmd: s:%d, of:%x" %  (pagesize + 12,  offset + flash_base)
      cmd = struct.pack(">III", pagesize + 12, 0, offset + flash_base)
      read = self.wr_command(cmd, read_count=0)

      todo = pagesize
      while todo > 0:
        packetsize = min(maxpacket, todo)
        data = self._binfile.read(packetsize)
        if len(data) != packetsize:
          raise Exception("Update", "No more data from file")
        for i in range(0, 10):
          try:
            self.wr_command(data, read_count=0)
            break
          except:
            print "Timeout fail"
        todo -= packetsize
      length -= pagesize
      offset += pagesize

      read = self.wr_command("", read_count=4)
      result = struct.unpack("<I", read)
      result = result[0]
      if result != 0:
        #print "Result is %s, read [%s]" % (result, read)
        raise Exception("Update", "Upload failed with rc: 0x%x" % result)

  def start(self):
    """Start programming.

    Args:
    """
    # 3 uint32 fields
    size = 3 * 4
    expected = 2 * 4


    cmd = struct.pack("<III", size, 0, 0)
    read = self.wr_command(cmd, read_count=expected)

    if len(read) == 4:
      raise Exception("Update", "Protocol version 0 not supported")
    elif len(read) == expected:
      base, version = struct.unpack(">II", read)
      print "base: %x, ver: %x" % (base, version)
    else:
      raise Exception("Update", "Start command returned %d bytes" % len(read))

    if base < 256:
      raise Exception("Update", "Start returned error code 0x%x" % base)

    self._base = base


  """Load firmware layout file.

  example as follows:
  {
    "board": "servo micro",
    "vid": 6353,
    "pid": 20506,
    "flash": 134217728,
    "regions": {
      "RW": [65536, 65536],
      "PSTATE": [63488, 2048],
      "RO": [0, 63488]
    }
  }
  """
  def load_board(self, brdfile):
    with open(brdfile) as data_file:
        data = json.load(data_file)

    #TODO: validate this.
    self._brdcfg = data;
    if debug:
      pprint(data)

    self._flashsize = 0
    for region in self._brdcfg['regions']:
      print "region %s\tbase:0x%08x size:0x%08x" % (region,
            self._brdcfg['regions'][region][0], self._brdcfg['regions'][region][1])
      self._flashsize += self._brdcfg['regions'][region][1]

    print "Flash Size: 0x%x" % self._flashsize

  def load_file(self, binfile):
    self._filesize = os.path.getsize(binfile)
    self._binfile = open(binfile)

    if self._filesize != self._flashsize:
      raise Exception("Update", "Flash size 0x%x != file size 0x%x" % (self._flashsize, self._filesize))




parser = argparse.ArgumentParser(description="Update firmware over usb")
parser.add_argument('-b', '--board', type=str, help="Board configuration json file", default="board.json")
parser.add_argument('-f', '--file', type=str, help="Complete ec.bin file", default="ec.bin")
parser.add_argument('-r', '--region', type=str, help="Region name to update", default="RW")
parser.add_argument('-s', '--serial', type=str, help="Serial number", default="")
parser.add_argument('-l', '--list', action="store_true", help="List regions")
parser.add_argument('-v', '--verbose', action="store_true", help="Chatty output")

def main():
  global debug
  args = parser.parse_args()


  brdfile = args.board
  region = args.region
  serial = args.serial
  binfile = args.file
  if args.verbose:
    debug = True

  with open(brdfile) as data_file:
    names = json.load(data_file)

  p = Supdate(serialname=serial)
  p.load_board(brdfile)
  p.load_file(binfile)

  p.start()
  p.write_file("RW")
  p.stop()

if __name__ == "__main__":
  main()



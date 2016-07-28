#!/usr/bin/python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for initializing and driving a TPM device.

Two kinds of connections are supported - one through a USB FTDI SPI adapter,
and another one - a direct connection to /dev/tpm.

The user does not have the ability to choose the interface, USB FTDI SPI is
tried first, and if it does not succeed /dev/tpm0 is tried.
"""

import hashlib
import os
import struct
import sys
import usb

import subcmd

# Suppressing pylint warning about an import not at the top of the file. The
# path needs to be set *before* the last import.
# pylint: disable=C6204
root_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
sys.path.append(os.path.join(root_dir, '..', '..', 'build', 'tpm_test'))
try:
  # Absence of this module indicates that /dev/tpm0 should be used.
  import ftdi_spi_tpm
except ImportError:
  pass

class FtdiSpi(object):
  def __init__(self):
    self._handle = ftdi_spi_tpm

  def Init(self, freq, debug_mode):
    if not self._handle.FtdiSpiInit(freq, debug_mode):
      raise subcmd.TpmTestError('Failed to connect to FTDI SPI')
    return True

  def SendAndReceive(self, cmd_data):
    return self._handle.FtdiSendCommandAndWait(cmd_data)

class UsbTpm(object):
  USB_SUBCLASS_GOOGLE_UPDATE = 0x53
  USB_CLASS_VENDOR = 0xFF
  CR50_VID = 0x18d1
  CR50_PID = 0x5014

  def __init__(self):
    """Initial discovery and connection to USB endpoint.

    This searches for a USB device matching the VID:PID specified
    in the config file, optionally matching a specified serialname.

    Args:
      serialname: Find the device with this serial, in case multiple
          devices are attached.

    Returns:
      True on success.
    Raises:
      Exception on error.
    """

    self._debug_mode = False

    dev_list = usb.core.find(idVendor=self.CR50_VID, idProduct=self.CR50_PID, find_all=True)
    if dev_list is None:
      raise subcmd.TpmTestError('Failed to connect over USB')

    if len(dev_list) != 1:
      raise subcmd.TpmTestError('%d devices found' % len(dev_list))

    dev = dev_list[0]
    self._dev = dev

    # Get an endpoint instance.
    try:
      dev.set_configuration()
    except:
      pass
    cfg = dev.get_active_configuration()

    self._intf = usb.util.find_descriptor(cfg, custom_match=lambda i: \
        i.bInterfaceClass==self.USB_CLASS_VENDOR and \
        i.bInterfaceSubClass==self.USB_SUBCLASS_GOOGLE_UPDATE)

    self._read_ep = usb.util.find_descriptor(
      self._intf,
      # match the first IN endpoint
      custom_match = \
      lambda e: \
        usb.util.endpoint_direction(e.bEndpointAddress) == \
        usb.util.ENDPOINT_IN
    )

    self._write_ep = usb.util.find_descriptor(
      self._intf,
      # match the first OUT endpoint
      custom_match = \
      lambda e: \
        usb.util.endpoint_direction(e.bEndpointAddress) == \
        usb.util.ENDPOINT_OUT
    )
    print('USB Device found')

  def Init(self, unused_, debug_mode):
    self._debug_mode = debug_mode
    self._write_ep.write(struct.pack('>3L', 12, 0, 0), 100)
    response = self._read_ep.read(2000, 1000)
    return len(response) == 40

  def SendAndReceive(self, cmd_data):
    base = struct.pack('>L', 0)
    sha1 = hashlib.sha1()
    sha1.update(base)  # Base set to zero.
    sha1.update(cmd_data)
    digest = sha1.digest()[:4]
    self._write_ep.write(struct.pack('>L', 4 + len(digest) +
                                     len(base) +
                                     len(cmd_data)) +
                         digest + base, 100)
    self._write_ep.write(cmd_data, 100)
    response = self._read_ep.read(2000, 1000)
    return struct.pack('>HLLH', 0x8001, len(response) + 12, 0, 0) + ''.join('%c' % x for x in response)


class DevTpm(object):
  _TPM_DEV_ = '/dev/tpm0'
  _MAX_RESPONSE_SIZE = 1024

  def __init__(self):
    self._debug = False
    try:
      self._fd = os.open(self._TPM_DEV_, os.O_RDWR)
    except OSError:
      raise subcmd.TpmTestError('Failed to open %s' % self._TPM_DEV_)

  def __del__(self):
    os.close(self._fd)

  def Init(self, unused_, debug_mode):
    self.debug_mode = debug_mode
    return True

  def SendAndReceive(self, cmd_data):
    count = 0
    while count != len(cmd_data):
      count += os.write(self._fd, cmd_data[count:])

    response = os.read(self._fd, self._MAX_RESPONSE_SIZE)
    return response

class TpmHandlerFactory(object):
  def GetHandler(self):
    try:
      return UsbTpm()
    except subcmd.TpmTestError, e:
      print e
    if 'ftdi_spi_tpm' in sys.modules:
      return FtdiSpi()
    else:
      return DevTpm()

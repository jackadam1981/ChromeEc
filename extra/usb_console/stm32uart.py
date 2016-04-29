#!/usr/bin/python 
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#"""Allow creation of uart/console interface via stm32 usb endpoint."""
import array
import errno
import exceptions
import logging
import os
import pty
import select
import sys
import termios
import threading
import time
import tty
import usb

import stm32usb


class SuartError(Exception):
  """Class for exceptions of Suart."""
  def __init__(self, msg, value=0):
    """SuartError constructor.

    Args:
      msg: string, message describing error in detail
      value: integer, value of error when non-zero status returned.  Default=0
    """
    super(SuartError, self).__init__(msg, value)
    self.msg = msg
    self.value = value


class Suart():
  """Provide interface to stm32 serial usb endpoint."""
  def __init__(self, vendor=0x18d1, product=0x501c, interface=0,
               serialname=None):
    """Suart contstructor.

    Initializes stm32 USB stream interface.

    Args:
      vendor: usb vendor id of stm32 device
      product: usb product id of stm32 device
      interface: interface number of stm32 device to use
      serialname: n/a. Defaults to None.
      ftdi_context: n/a. Defaults to None.

    Raises:
      SuartError: If init fails
    """
    #self._logger.debug('Suart opening %04x:%04x, intf %d, sn: %s' % (
    #    vendor, product, interface, serialname))

    self._susb = stm32usb.Susb(vendor=vendor, product=product,
        interface=interface, serialname=serialname)
    self._exit = False

    #self._logger.debug("Set up stm32 uart")

  def __del__(self):
    """Suart destructor."""
    pass

  def run_rx_thread(self):
    #self._logger.debug('rx thread started on %s' % self.get_pty())

    while True:
        try:
          r = self._susb._read_ep.read(64, self._susb.TIMEOUT_MS)
          if r:
            #print "%s" % r.tostring()
            sys.stdout.write(r.tostring())
            sys.stdout.flush()

        except Exception as e:
          # If we miss some characters on pty disconnect, that's fine.
          # ep.read() also throws USBError on timeout, which we discard.
          if type(e) not in [exceptions.OSError, usb.core.USBError]:
            print "rx %s" % e

  def run_tx_thread(self):
    #self._logger.debug("tx thread started on %s" % self.get_pty())

    while True:
        try:
          r = sys.stdin.read(1)
          if r == '\x03':
            force_exit()
          if r:
            self._susb._write_ep.write(array.array('B', r), self._susb.TIMEOUT_MS)

        except Exception as e:
          print "tx %s" % e


  def run(self):
    """Creates pthreads to poll stm32 & PTY for data.
    """

    self._rx_thread = threading.Thread(target=self.run_rx_thread, args=[])
    self._rx_thread.daemon = True
    self._rx_thread.start()

    self._tx_thread = threading.Thread(target=self.run_tx_thread, args=[])
    self._tx_thread.daemon = True
    self._tx_thread.start()

    print 'stm32 rx and tx threads started.'


  def get_pty(self):
    """Gets path to pty for communication to/from uart.

    Returns:
      String path to the pty connected to the uart
    """
    return ""

def force_exit():
  global old_settings
  global fd
  termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
  os.system("stty echo")
  sys.exit(0)

def main():
  sobj = Suart()
  sobj.run()

  # run() is a thread so just busy wait to mimic server
  while True:
    # ours sleeps to eleven!
    time.sleep(.1)

if __name__ == '__main__':
  global old_settings
  global fd
  os.system("stty -echo")
  fd = sys.stdin.fileno()
  old_settings = termios.tcgetattr(fd)
  tty.setraw(sys.stdin.fileno())
  try:
    main()
  except KeyboardInterrupt:
    print "exit"
  finally:
    force_exit()

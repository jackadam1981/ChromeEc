#!/usr/bin/python
# Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Quick and dirty utility to generate traffic on the USB PHY.

Must be used in conjunction with the usb_echo code on the DUT.
"""


CR50_VID = 0x18d1
CR50_PID = 0x5014

INTERFACE_NUM = 1
EP_NUM = 2
EP_IN  = 0x80 | EP_NUM

import usb
import sys
import time

def get_usb_hnd():
    """
    Open CR50 USB device and return a handle to the USB object.
    """

    busses = usb.busses()

    hnd = None
    for bus in busses:
        devices = bus.devices
        for dev in devices:
            if dev.idVendor == CR50_VID and dev.idProduct == CR50_PID:
                print "USB DEV %04x:%04x" % (dev.idVendor, dev.idProduct)
                hnd = dev.open()
                break
    if not hnd:
        sys.stderr.write("NO USB device found\n")
        sys.exit(1)
    return hnd

def rx_test(hnd):
    """
    Try to get as many IN packets from the DUT as possible.
    """
    print "RX TEST..."
    hnd.bulkWrite(EP_NUM,"START_RX")
    while True:
        total = 0
        tm0 = time.time()
        while time.time() - tm0 < 1:
            pkt = hnd.bulkRead(EP_IN, 10*1024)
            total += len(pkt)
        tm1 = time.time()
        print "RX Speed %d kB/s" % (total/(tm1-tm0)/1024)

def tx_test(hnd):
    """
    Try to send as many OUT packets to the DUT as possible.
    """
    print "TX TEST..."
    pattern = "".join([ chr(ord('0')+(i&7)) for i in range(64)])
    large_pattern = pattern*160
    while True:
        total = 0
        tm0 = time.time()
        while time.time() - tm0 < 1:
            hnd.bulkWrite(EP_NUM, large_pattern)
            total += len(large_pattern)
        tm1 = time.time()
        print "TX Speed %d kB/s" % (total/(tm1-tm0)/1024)

def run_test(mode):
    """
    Test the CR50 DUT by sending/receiving a larger number of USB packets.
    """
    hnd = get_usb_hnd()
    try:
        hnd.claimInterface(INTERFACE_NUM)
    except usb.core.USBError:
        hnd.detachKernelDriver(INTERFACE_NUM)
        hnd.claimInterface(INTERFACE_NUM)
    if "tx" in mode:
        tx_test(hnd)
    else:
        rx_test(hnd)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: %s [rx|tx]\n" % sys.argv[0])
        sys.exit(1)
    run_test(sys.argv[1])

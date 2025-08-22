# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from enum import IntEnum

import usb.core


GOOGLE_VID = 0x18D1
USB_SUBCLASS_GOOGLE_EC_HOST_CMD = 0x5A
USB_BCC_VENDOR = 0xFF


class HostCommandIRQType(IntEnum):
    """Event types sent via the interrupt endpoint"""

    EVENT = 0
    RESPONSE_READY = 1


class FindHCInf:
    def __call__(self, device):
        # first, let's check the device
        for cfg in device:
            # find_descriptor: what's it?
            intf = usb.util.find_descriptor(
                cfg,
                bInterfaceClass=USB_BCC_VENDOR,
                bInterfaceSubClass=USB_SUBCLASS_GOOGLE_EC_HOST_CMD,
            )
            if intf is not None:
                return True

        return False


class UsbCommunication:
    """Base class for USB Communication."""

    def __init__(self):
        self.ec_dev = usb.core.find(
            idVendor=GOOGLE_VID, custom_match=FindHCInf()
        )
        if self.ec_dev is None:
            raise Exception("Failed to find HC interface")

        cfg = self.ec_dev.get_active_configuration()
        intf = usb.util.find_descriptor(
            cfg,
            bInterfaceClass=USB_BCC_VENDOR,
            bInterfaceSubClass=USB_SUBCLASS_GOOGLE_EC_HOST_CMD,
        )

        self.ep_out = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_direction(
                ep.bEndpointAddress
            )
            == usb.util.ENDPOINT_OUT,
        )
        self.ep_in_bulk = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_direction(
                ep.bEndpointAddress
            )
            == usb.util.ENDPOINT_IN
            and usb.util.endpoint_type(ep.bmAttributes)
            == usb.util.ENDPOINT_TYPE_BULK,
        )
        self.ep_in_int = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_type(ep.bmAttributes)
            == usb.util.ENDPOINT_TYPE_INTR,
        )

        if self.ec_dev.is_kernel_driver_active(intf.bInterfaceNumber):
            raise Exception("Interface active")

    def __del__(self):
        if self.ec_dev:
            usb.util.dispose_resources(self.ec_dev)

    def send(self, cmd_bytes) -> int:
        return self.ep_out.write(cmd_bytes, timeout=100)

    def wait(self):
        # Wait for response ready signal from interrupt EP
        while True:
            ret = self.ep_in_int.read(
                self.ep_in_int.wMaxPacketSize, timeout=200
            )
            if ret[0] == HostCommandIRQType.RESPONSE_READY:
                break

    def receive(self, size=-1):
        if size < 0:
            size = self.ep_in_bulk.wMaxPacketSize
        return self.ep_in_bulk.read(size, timeout=100)


class FakeCommunication:
    def send(self, cmd_bytes) -> int:
        return len(cmd_bytes)

    def receive(self, response_len):
        ret = bytearray(response_len)
        return ret

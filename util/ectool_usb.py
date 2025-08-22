# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from abc import ABC
import argparse
from collections import namedtuple
from enum import IntEnum
import struct
import sys
import time

import usb.core


GOOGLE_VID = 0x18D1
USB_SUBCLASS_GOOGLE_EC_HOST_CMD = 0x5A
USB_BCC_VENDOR = 0xFF


class ImageType(IntEnum):
    """EC Image type"""

    RO = 1
    RW = 2

    @staticmethod
    def get_image_name(image: int) -> str:
        if image == ImageType.RO:
            return "RO"
        elif image == ImageType.RW:
            return "RW"
        else:
            return "Unknown"


class ECRebootCmd(IntEnum):
    CANCEL = 0
    JUMP_RO = 1
    JUMP_RW = 2
    COLD = 4
    DISABLE_JUMP = 5
    HIBERNATE = 6
    HIBERNATE_CLEAR_AP_OFF = 7
    COLD_AP_OFF = 8
    NO_OP = 9

    def __str__(self):
        return self.name

    @staticmethod
    def from_string(s):
        return ECRebootCmd[s]


class FlashRegion(IntEnum):
    RO = 0
    ACTIVE = 1
    WP_RO = 2
    UPDATE = 3

    def __str__(self):
        return self.name

    @staticmethod
    def from_string(s):
        return FlashRegion[s]


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


class HostCommand:
    """Base class for Host Command."""

    cmd_bytes: bytes
    request_fmt: str = ""
    response_fmt: str = ""
    response_type: type = None

    def __init__(self, cmd_id, cmd_ver=0, payload: bytes = b""):
        self.cmd_bytes = HostCommand.pack_command(cmd_id, cmd_ver, payload)
        self.response = None

    @staticmethod
    def update_checksum(cmd: bytes) -> bytes:
        # Make sure current checksum is 0
        cmd_list = list(cmd)
        cmd_list[1] = 0
        checksum = 0
        for x in cmd_list:
            checksum += x

        cmd_list[1] = (256 - checksum % 256) % 256

        return bytes(cmd_list)

    @staticmethod
    def checksum_valid(cmd: bytes) -> bool:
        # Make sure current checksum is 0
        cmd_list = list(cmd)
        checksum = 0
        for x in cmd_list:
            checksum += x

        return checksum % 256 == 0

    @staticmethod
    def pack_command(cmd_id: int, cmd_ver: int, payload: bytes = b"") -> bytes:
        # 1 byte - protocol version
        # 1 byte - checksum
        # 2 bytes - command id
        # 1 byte - command version
        # 1 byte - reserved
        # 2 bytes - data length
        cmd_header = struct.pack(
            "<BBHBBH", 3, 0, cmd_id, cmd_ver, 0, len(payload)
        )

        cmd = cmd_header + payload
        cmd = HostCommand.update_checksum(cmd)

        return cmd

    def send_cmd(self) -> int:
        ec_dev = usb.core.find(idVendor=GOOGLE_VID, custom_match=FindHCInf())
        if ec_dev is None:
            print("Failed to find HC interface")
            return -1

        cfg = ec_dev.get_active_configuration()
        intf = usb.util.find_descriptor(
            cfg,
            bInterfaceClass=USB_BCC_VENDOR,
            bInterfaceSubClass=USB_SUBCLASS_GOOGLE_EC_HOST_CMD,
        )

        ep_out = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_direction(
                ep.bEndpointAddress
            )
            == usb.util.ENDPOINT_OUT,
        )
        ep_in_bulk = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_direction(
                ep.bEndpointAddress
            )
            == usb.util.ENDPOINT_IN
            and usb.util.endpoint_type(ep.bmAttributes)
            == usb.util.ENDPOINT_TYPE_BULK,
        )
        ep_in_int = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_type(ep.bmAttributes)
            == usb.util.ENDPOINT_TYPE_INTR,
        )

        if ec_dev.is_kernel_driver_active(intf.bInterfaceNumber):
            print("Interface active")
            return -1

        # Header + payload
        response_len = struct.calcsize(self.response_fmt) + 8
        try:
            ret = ep_out.write(self.cmd_bytes, timeout=100)
            if ret != len(self.cmd_bytes):
                print("Failed to send: " + str(ret))

            # Wait for response ready signal from interrupt EP
            while True:
                ret = ep_in_int.read(ep_in_int.wMaxPacketSize, timeout=200)
                if ret[0] == HostCommandIRQType.RESPONSE_READY:
                    break

            # Get response from bulk EP
            ret = ep_in_bulk.read(response_len, timeout=100)
            usb.util.dispose_resources(ec_dev)
        except Exception as e:
            print("Communication error")
            print(f"{e}")
            return -1

        if len(ret) < 8:
            print("Invalid response len: " + str(len(ret)))
            return -2

        if not HostCommand.checksum_valid(ret):
            print("Response checksum invalid")
            return -3

        try:
            response_header = struct.unpack("<bbhhh", ret[:8])
        except Exception as e:
            print("Failed to unpack header")
            print(f"{e}")
            return -4

        if response_header[2] != 0:
            return response_header[2]

        if response_len != len(ret):
            print(
                "Invalide response length. Received: "
                + str(len(ret))
                + " expected: "
                + str(response_len)
            )

        if response_header[3] != response_len - 8:
            print(
                "Invalide header length. Header: "
                + str(response_header[3])
                + " expected: "
                + str(struct.calcsize(self.response_fmt))
            )

        try:
            if len(ret) > 8:
                response_unnamed = struct.unpack(self.response_fmt, ret[8:])
                self.response = self.response_type(*response_unnamed)
        except Exception as e:
            print("Failed to unpack response payload")
            print(f"{e}")
            return -5

        return 0

    def run(self) -> int:
        """Run the host command."""

        return self.send_cmd()


class GetVersionCmd(HostCommand):

    def __init__(self):
        super().__init__(0x0002, 1)
        # 32 bytes of RO string version
        # 32 bytes of RW string version
        # 32 bytes of fwid_ro
        # 4 bytes of current image
        # 32 bytes of fwid_rw
        self.response_fmt = "<32s32s32sI32s"
        self.response_type = namedtuple(
            "Response", ["ro_ver", "rw_ver", "fwid_ro", "curr_image", "fwid_rw"]
        )


class FpInfoCmd(HostCommand):

    def __init__(self):
        super().__init__(0x0403, 1)
        # 4 bytes of vendor id
        # 4 bytes of product id
        # 4 bytes of model id
        # 4 bytes of version
        # 4 bytes of frame size
        # 4 bytes of pixel format
        # 2 bytes of width
        # 2 bytes of height
        # 2 bytes of bpp
        # 2 bytes of errors
        # 4 bytes of template size
        # 2 bytes of template max
        # 2 bytes of template valid
        # 4 bytes of template dirty
        # 4 bytes of template version
        self.response_fmt = "<IIIIIIHHHHIHHII"
        self.response_type = namedtuple(
            "Response",
            [
                "vendor_id",
                "product_id",
                "model_id",
                "version",
                "frame_size",
                "pixel_format",
                "width",
                "height",
                "bpp",
                "errors",
                "template_size",
                "template_max",
                "template_valid",
                "template_dirty",
                "template_version",
            ],
        )


class EnterBootloaderCmd(HostCommand):
    def __init__(self):
        super().__init__(0x00E2)


class FlashReadCmd(HostCommand):

    def __init__(self, offset: int, size: int):
        # "size" bytes of data

        self.response_fmt = f"{size}s"
        self.response_type = namedtuple("Response", ["data"])

        # 4 bytes of offset
        # 4 bytes of size
        payload = struct.pack("<II", offset, size)

        super().__init__(0x0011, 0, payload=payload)


class FlashWriteCmd(HostCommand):

    def __init__(self, offset: int, data: bytes):
        size = len(data)
        # 4 bytes of offset
        # 4 bytes of size
        payload = struct.pack(f"<II{size}s", offset, size, data)

        super().__init__(0x0012, 0, payload=payload)


class FlashEraseCmd(HostCommand):
    def __init__(self, offset: int, size: int):
        # 4 bytes of offset
        # 4 bytes of size
        payload = struct.pack("<II", offset, size)

        super().__init__(0x0013, 0, payload=payload)


class FlashRegionInfoCmd(HostCommand):
    def __init__(self, region: int):
        # 4 bytes of region
        payload = struct.pack("<I", region)

        # 4 bytes of offset
        # 4 bytes of size
        self.response_fmt = "<II"
        self.response_type = namedtuple("Response", ["offset", "size"])
        super().__init__(0x0016, 1, payload=payload)


class RebootECCmd(HostCommand):
    def __init__(self, cmd: int, flags: int = 0):
        # 1 byte of command
        # 1 byte of flags
        payload = struct.pack("<BB", cmd, flags)

        super().__init__(0x00D2, 0, payload=payload)


def cmd_get_version(args) -> int:
    get_ver = GetVersionCmd()
    ret = get_ver.run()
    if ret != 0:
        return ret

    print("RO: " + get_ver.response.ro_ver.decode("ascii"))
    print("RW: " + get_ver.response.rw_ver.decode("ascii"))
    print("FWID_RO: " + get_ver.response.fwid_ro.decode("ascii"))
    print(
        "Current image: "
        + ImageType.get_image_name(get_ver.response.curr_image)
    )
    print("FWID_RW: " + get_ver.response.fwid_rw.decode("ascii"))


def cmd_fp_info(args) -> int:
    fp_info = FpInfoCmd()
    ret = fp_info.run()
    if ret != 0:
        return ret

    print("Vendor ID: " + hex(fp_info.response.vendor_id))
    print("Product ID: " + hex(fp_info.response.product_id))
    print("Model ID: " + hex(fp_info.response.model_id))
    print("Version: " + hex(fp_info.response.version))
    print("Frame size: " + str(fp_info.response.frame_size))
    print("Pixel format: " + hex(fp_info.response.pixel_format))
    print("Width: " + str(fp_info.response.width))
    print("Height: " + str(fp_info.response.height))
    print("BPP: " + str(fp_info.response.bpp))
    print("Error: " + hex(fp_info.response.errors))
    print("Template size: " + str(fp_info.response.template_size))
    print("Template max: " + str(fp_info.response.template_max))
    print("Template valid: " + str(fp_info.response.template_valid))
    print("Template dirty: " + hex(fp_info.response.template_dirty))
    print("Template version: " + hex(fp_info.response.template_version))


def cmd_enter_bootloader(args) -> int:
    enter_bootloader = EnterBootloaderCmd()
    return enter_bootloader.run()


def flash_read_to_file(file: str, offset: int, size: int):
    read_bytes = 0
    ret = 0
    with open(file, "wb") as file:
        while read_bytes < size:
            remaining_bytes = size - read_bytes
            if remaining_bytes > 256:
                chunk = 256
            else:
                chunk = remaining_bytes

            flash_read = FlashReadCmd(offset=offset + read_bytes, size=chunk)
            ret = flash_read.run()
            if ret != 0:
                return ret
            file.write(flash_read.response.data)
            read_bytes += chunk

    return ret


def cmd_flash_read(args) -> int:
    return flash_read_to_file(args.file, args.offset, args.size)


def flash_write_from_file(file: str, offset: int):
    with open(file, "rb") as file:
        data = file.read()

    ret = 0
    size = len(data)
    written_bytes = 0

    while written_bytes < size:
        remaining_bytes = size - written_bytes
        if remaining_bytes > 256:
            chunk = 256
        else:
            chunk = remaining_bytes

        flash_write = FlashWriteCmd(
            offset=offset + written_bytes,
            data=data[written_bytes : written_bytes + chunk],
        )
        ret = flash_write.run()
        if ret != 0:
            return ret
        written_bytes += chunk

    return ret


def cmd_flash_write(args) -> int:
    return flash_write_from_file(args.file, offset=args.offset)


def cmd_flash_erase(args) -> int:
    flash_erase = FlashEraseCmd(offset=args.offset, size=args.size)
    return flash_erase.run()


def cmd_flash_region_info(args) -> int:
    flash_region_info = FlashRegionInfoCmd(region=args.region)
    ret = flash_region_info.run()

    if ret != 0:
        return ret

    print("Offset: " + hex(flash_region_info.response.offset))
    print("Size: " + hex(flash_region_info.response.size))


def cmd_reboot_ec(args) -> int:
    reboot_ec = RebootECCmd(cmd=args.cmd)
    return reboot_ec.run()


def cmd_reflash_rw(args) -> int:
    reboot_ec = RebootECCmd(cmd=ECRebootCmd.JUMP_RO)
    ret = reboot_ec.run()
    if ret != 0:
        print("Failed to jump to RO")
        return ret

    time.sleep(1)
    get_ver = GetVersionCmd()
    ret = get_ver.run()
    if ret != 0:
        return ret

    if get_ver.response.curr_image != ImageType.RO:
        print("Failed to stay in RO")
        return -1

    print("Stayed in RO after sysjump")

    flash_region_info = FlashRegionInfoCmd(region=FlashRegion.UPDATE)
    ret = flash_region_info.run()
    if ret != 0:
        print("Failed to get RW info")
        return ret

    rw_file = "rw_tmp.bin"
    ret = flash_read_to_file(
        rw_file,
        flash_region_info.response.offset,
        flash_region_info.response.size,
    )
    if ret != 0:
        print("Failed to read RW")

    print(f"RW stored in {rw_file}")

    erase_offset = flash_region_info.response.offset
    while (
        erase_offset
        < flash_region_info.response.offset + flash_region_info.response.size
    ):
        chunk = 1024 * 8
        flash_erase = FlashEraseCmd(offset=erase_offset, size=chunk)
        ret = flash_erase.run()
        # In progress
        if ret == 8:
            time.sleep(0.1)
        elif ret != 0:
            print("Failed to flash erase: " + hex(erase_offset))
            return ret

        erase_offset += chunk

    print("RW erased")
    reboot_ec = RebootECCmd(cmd=ECRebootCmd.COLD)
    ret = reboot_ec.run()
    if ret != 0:
        print("Failed to reboot after erase")
        return ret

    time.sleep(3)
    ret = get_ver.run()
    if ret != 0:
        return ret

    if get_ver.response.curr_image != ImageType.RO:
        print("Failed to stay in RO after erase")
        return -1

    print("Stayed in RO after reboot")
    flash_write_from_file(rw_file, flash_region_info.response.offset)
    print("RW re-written")

    ret = reboot_ec.run()
    if ret != 0:
        print("Failed to reboot after RW write")
        return ret

    time.sleep(3)
    ret = get_ver.run()
    if ret != 0:
        return ret

    if get_ver.response.curr_image != ImageType.RW:
        print("Failed to jump to RW")
        return -1

    print("Done")


def add_arg_file(sub_parser: argparse.ArgumentParser):
    sub_parser.add_argument("file", type=str)


def add_arg_offset(sub_parser: argparse.ArgumentParser):
    sub_parser.add_argument("offset", type=int)


def add_arg_size(sub_parser: argparse.ArgumentParser):
    sub_parser.add_argument("size", type=int)


def main():
    parser = argparse.ArgumentParser(
        description="USB version of ectool",
    )

    subcmd = parser.add_subparsers(
        dest="command", help="Host command to run", required=True
    )

    sub_get_version = subcmd.add_parser("version", help="Get version")
    sub_get_version.set_defaults(func=cmd_get_version)

    sub_fp_info = subcmd.add_parser("fpinfo", help="FP info")
    sub_fp_info.set_defaults(func=cmd_fp_info)

    sub_flash_read = subcmd.add_parser("flashread", help="Flash read")
    add_arg_offset(sub_flash_read)
    add_arg_size(sub_flash_read)
    add_arg_file(sub_flash_read)
    sub_flash_read.set_defaults(func=cmd_flash_read)

    sub_flash_write = subcmd.add_parser("flashwrite", help="Flash write")
    add_arg_offset(sub_flash_write)
    add_arg_file(sub_flash_write)
    sub_flash_write.set_defaults(func=cmd_flash_write)

    sub_flash_erase = subcmd.add_parser("flasherase", help="Flash erase")
    add_arg_offset(sub_flash_erase)
    add_arg_size(sub_flash_erase)
    sub_flash_erase.set_defaults(func=cmd_flash_erase)

    sub_flash_region_info = subcmd.add_parser(
        "flashregioninfo", help="Flash region info"
    )
    # TODO improve getting choices from enums?
    sub_flash_region_info.add_argument(
        "region",
        type=FlashRegion.from_string,
        choices=list(FlashRegion),
        help="Region",
    )
    sub_flash_region_info.set_defaults(func=cmd_flash_region_info)

    sub_enter_bootloader = subcmd.add_parser(
        "bootloader", help="Enter bootloader"
    )
    sub_enter_bootloader.set_defaults(func=cmd_enter_bootloader)

    sub_reboot_ec = subcmd.add_parser("reboot_ec", help="Reboot EC")
    # TODO improve getting choices from enums?
    sub_reboot_ec.add_argument(
        "cmd",
        type=ECRebootCmd.from_string,
        choices=list(ECRebootCmd),
        help="Reboot command",
    )
    sub_reboot_ec.set_defaults(func=cmd_reboot_ec)

    sub_reflash_rw = subcmd.add_parser("reflash_rw", help="Try reflashing rw")
    sub_reflash_rw.set_defaults(func=cmd_reflash_rw)

    args = parser.parse_args()

    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

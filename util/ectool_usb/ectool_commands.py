# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
from enum import IntEnum
import struct


# 1 byte - protocol version
# 1 byte - checksum
# 2 bytes - command id
# 1 byte - command version
# 1 byte - reserved
# 2 bytes - data length
REQUEST_HEADER_FMT = "<BBHBBH"
REQUEST_HEADER_LEN = struct.calcsize(REQUEST_HEADER_FMT)
# 1 byte - protocol version
# 1 byte - checksum
# 2 bytes - result
# 2 bytes - data length
# 2 bytes - reserved
RESPONSE_HEADER_FMT = "<bbhhh"
RESPONSE_HEADER_LEN = struct.calcsize(RESPONSE_HEADER_FMT)
RESPONSE_HEADER_TYPE = namedtuple(
    "Header",
    [
        "protocol_version",
        "checksum",
        "result",
        "data_length",
        "reserved",
    ],
)


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


class HostCommand:
    """Base class for Host Command."""

    cmd_bytes: bytes
    request_fmt: str = ""
    response_fmt: str = ""
    response_type: type = None
    variable_response: bool = False
    cmd_version: int = 0

    def __init__(self, cmd_id, cmd_ver=0, payload: bytes = b""):
        self.cmd_bytes = HostCommand.pack_command(cmd_id, cmd_ver, payload)
        self.cmd_version = cmd_ver
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
        cmd_header = struct.pack(
            REQUEST_HEADER_FMT, 3, 0, cmd_id, cmd_ver, 0, len(payload)
        )

        cmd = cmd_header + payload
        cmd = HostCommand.update_checksum(cmd)

        return cmd

    def send_cmd(self, comm) -> int:
        # Header + payload
        response_len = struct.calcsize(self.response_fmt) + RESPONSE_HEADER_LEN
        try:
            ret = comm.send(self.cmd_bytes)
            if ret != len(self.cmd_bytes):
                print("Failed to send: " + str(ret))
            # Wait fot the response
            comm.wait()
            # Get header and first chunk of payload
            ret = comm.receive()
            # Unpack header to get size
            try:
                response_header = RESPONSE_HEADER_TYPE(
                    *struct.unpack(
                        RESPONSE_HEADER_FMT, ret[:RESPONSE_HEADER_LEN]
                    )
                )
            except Exception as e:
                print("Failed to unpack header")
                print(f"{e}")
                return -4
            # Get rest of the message
            if response_header.data_length > len(ret) - RESPONSE_HEADER_LEN:
                ret += comm.receive(
                    response_header.data_length
                    - (len(ret) - RESPONSE_HEADER_LEN)
                )
            # Adjust expected length and format for variable size messages
            if self.variable_response:
                response_len = len(ret)
                self.response_fmt += f"{response_len - RESPONSE_HEADER_LEN - struct.calcsize(self.response_fmt)}s"

        except Exception as e:
            print("Communication error")
            print(f"{e}")
            return -1

        if len(ret) < RESPONSE_HEADER_LEN:
            print("Invalid response len: " + str(len(ret)))
            return -2

        if not HostCommand.checksum_valid(ret):
            print("Response checksum invalid")
            return -3

        if response_header.result != 0:
            return response_header.result

        if response_len != len(ret):
            print(
                "Invalide response length. Received: "
                + str(len(ret))
                + " expected: "
                + str(response_len)
            )

        if response_header.data_length != response_len - RESPONSE_HEADER_LEN:
            print(
                "Invalide header length. Header: "
                + str(response_header.data_length)
                + " expected: "
                + str(struct.calcsize(self.response_fmt))
            )

        try:
            if len(ret) > RESPONSE_HEADER_LEN:
                response_unnamed = struct.unpack(
                    self.response_fmt, ret[RESPONSE_HEADER_LEN:]
                )
                self.response = self.response_type(*response_unnamed)
        except Exception as e:
            print("Failed to unpack response payload")
            print(f"{e}")
            return -5

        return 0

    def process_payload(self, payload_fmt, payload_type, payload_field):
        payload = getattr(self.response, payload_field)
        element_len = struct.calcsize(payload_fmt)
        elemnt_nr = int(len(payload) / element_len)
        new_payload_field = [
            payload_type(
                *struct.unpack_from(
                    payload_fmt,
                    payload,
                    i * element_len,
                )
            )
            for i in range(elemnt_nr)
        ]
        self.response = self.response._replace(
            **{payload_field: new_payload_field}
        )

    def run(self, comm) -> int:
        """Run the host command."""
        return self.send_cmd(comm)


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


class GetVersionsCmd(HostCommand):

    def __init__(self, command):
        # 4 bytes of version mask
        self.response_fmt = "<I"
        self.response_type = namedtuple("Response", ["version_mask"])
        # 2 bytes of cmd
        payload = struct.pack("<H", command)
        super().__init__(0x0008, 1, payload=payload)


class FlashInfoCmd(HostCommand):
    def __init__(self, num_banks_desc: int):
        # 4 bytes of flash_size
        # 4 bytes of flags
        # 4 bytes of write_ideal_size
        # 2 bytes of num_banks_total
        # 2 bytes of num_banks_desc
        # num_banks_desc of ec_flash_bank struct
        self.variable_response = True
        self.response_fmt = "<IIIHH"
        self.response_type = namedtuple(
            "Response",
            [
                "flash_size",
                "flags",
                "write_ideal_size",
                "num_banks_total",
                "num_banks_desc",
                "ec_flash_banks",
            ],
        )
        # 2 bytes of num_banks
        # 2 bytes reserved
        payload = struct.pack("<HH", num_banks_desc, 0)
        super().__init__(0x0010, 2, payload=payload)

    def run(self, comm) -> int:
        ret = super().run(comm)
        if ret == 0:
            # 2 bytes of count
            # 1 byte of size_exp
            # 1 byte of write_size_exp
            # 1 byte of erase_size_exp
            # 1 byte of protect_size_exp
            # 2 bytes of reserved
            ec_flash_bank_fmt = "<HBBBBH"
            ec_flash_bank_type = namedtuple(
                "Response",
                [
                    "count",
                    "size_exp",
                    "write_size_exp",
                    "erase_size_exp",
                    "protect_size_exp",
                    "reserved",
                ],
            )
            self.process_payload(
                ec_flash_bank_fmt, ec_flash_bank_type, "ec_flash_banks"
            )
        return ret


class ProtocolInfoCmd(HostCommand):
    def __init__(self):
        super().__init__(0x000B)
        # 4 bytes of protocol version
        # 2 bytes of max_request_packet_size
        # 2 bytes of max_response_packet_size
        # 4 bytes of flags
        self.response_fmt = "<IHHI"
        self.response_type = namedtuple(
            "Response",
            [
                "protocol_versions",
                "max_request_packet_size",
                "max_response_packet_size",
                "flags",
            ],
        )


class FpModeCmd(HostCommand):

    def __init__(self, mode: int):
        # 4 bytes of template version
        self.response_fmt = "<I"
        self.response_type = namedtuple(
            "Response",
            [
                "mode",
            ],
        )
        # 4 bytes of param1
        payload = struct.pack("<I", mode)
        super().__init__(0x0402, 0, payload=payload)


def get_fp_info_cmd(get_versions) -> HostCommand:
    versions = get_versions(0x0403)
    if 2 in versions:
        return FpInfoCmd2()
    elif 1 in versions:
        return FpInfoCmd1()
    return None


class FpInfoCmd1(HostCommand):

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


class FpInfoCmd2(HostCommand):

    def __init__(self):
        super().__init__(0x0403, 2)
        # 4 bytes of vendor id
        # 4 bytes of product id
        # 4 bytes of model id
        # 4 bytes of version
        # 2 bytes of num of capture types
        # 2 bytes of errors
        # 4 bytes of template size
        # 2 bytes of template max
        # 2 bytes of template valid
        # 4 bytes of template dirty
        # 4 bytes of template version
        # unknown number of image_frame
        self.variable_response = True
        self.response_fmt = "<IIIIHHIHHII"
        self.response_type = namedtuple(
            "Response",
            [
                "vendor_id",
                "product_id",
                "model_id",
                "version",
                "num_capture_types",
                "errors",
                "template_size",
                "template_max",
                "template_valid",
                "template_dirty",
                "template_version",
                "image_frame_params",
            ],
        )

    def run(self, comm) -> int:
        ret = super().run(comm)
        if ret == 0:
            # fp_image_frame_params
            # 4 bytes of frame_size;
            # 4 bytes of pixel_format;
            # 2 bytes of width;
            # 2 bytes of height;
            # 2 bytes of bpp;
            # 1 byte of fp_capture_type;
            # 1 byte of reserved;
            image_frame_fmt = "<IIHHHBB"
            image_frame_type = namedtuple(
                "Response",
                [
                    "frame_size",
                    "pixel_format",
                    "width",
                    "height",
                    "bpp",
                    "fp_capture_type",
                    "reserved",
                ],
            )
            self.process_payload(
                image_frame_fmt, image_frame_type, "image_frame_params"
            )
        return ret


class FpVendorCmd(HostCommand):

    def __init__(self, param1: int):
        # Variable number of bytes in response
        self.variable_response = True
        self.response_fmt = "<"
        self.response_type = namedtuple(
            "Response",
            [
                "payload",
            ],
        )
        # 4 bytes of param1
        payload = struct.pack("<I", param1)
        super().__init__(0x040B, 0, payload=payload)


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

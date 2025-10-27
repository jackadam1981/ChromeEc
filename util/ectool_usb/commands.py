# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for EC host commands."""

from collections import namedtuple
from enum import IntEnum
import struct
from typing import Callable, Optional


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
ResponseHeaderType = namedtuple(
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
        """Returns the name of the image."""
        if image == ImageType.RO:
            return "RO"
        if image == ImageType.RW:
            return "RW"
        return "Unknown"


class ECRebootCmd(IntEnum):
    """EC Reboot commands."""

    CANCEL = 0
    JUMP_RO = 1
    JUMP_RW = 2
    COLD = 4
    DISABLE_JUMP = 5
    HIBERNATE = 6
    HIBERNATE_CLEAR_AP_OFF = 7
    COLD_AP_OFF = 8
    NO_OP = 9

    def __str__(self) -> str:
        return self.name

    @staticmethod
    def from_string(s) -> "ECRebootCmd":
        """Returns reboot command from a string."""
        return ECRebootCmd[s]


class FlashRegion(IntEnum):
    """Flash regions."""

    RO = 0
    ACTIVE = 1
    WP_RO = 2
    UPDATE = 3

    def __str__(self) -> str:
        """Returns the name of the region."""
        return self.name

    @staticmethod
    def from_string(s) -> "FlashRegion":
        """Returns the region from a string."""
        return FlashRegion[s]


class HostCommandError(Exception):
    """Host command error."""


class EcCommandResult(IntEnum):
    """EC command result codes."""

    SUCCESS = 0
    INVALID_COMMAND = 1
    ERROR = 2
    INVALID_PARAM = 3
    ACCESS_DENIED = 4
    INVALID_RESPONSE = 5
    INVALID_VERSION = 6
    INVALID_CHECKSUM = 7
    IN_PROGRESS = 8
    UNAVAILABLE = 9
    TIMEOUT = 10
    OVERFLOW = 11
    INVALID_HEADER = 12
    REQUEST_TRUNCATED = 13
    RESPONSE_TOO_BIG = 14
    BUS_ERROR = 15
    BUSY = 16
    INVALID_HEADER_VERSION = 17
    INVALID_HEADER_CRC = 18
    INVALID_DATA_CRC = 19
    DUP_UNAVAILABLE = 20
    UNKNOWN = -1


class HostCommand:
    """Base class for Host Command."""

    cmd_bytes: bytes
    request_fmt: str = ""
    response_fmt: str = ""
    response_type: Optional[Callable] = None
    variable_response: bool = False
    cmd_version: int = 0

    def __init__(
        self,
        cmd_id,
        cmd_ver=0,
        payload: bytes = b"",
        variable_response: bool = False,
    ):
        """Initializes the host command."""
        self.cmd_bytes = HostCommand.pack_command(cmd_id, cmd_ver, payload)
        self.cmd_version = cmd_ver
        self.variable_response = variable_response
        self.response = None

    @staticmethod
    def update_checksum(cmd: bytes) -> bytes:
        """Updates the checksum of the command."""
        # Make sure current checksum is 0
        assert len(cmd) >= REQUEST_HEADER_LEN, "To Short request"
        cmd_list = list(cmd)
        cmd_list[1] = 0
        checksum = 0
        for x in cmd_list:
            checksum += x

        cmd_list[1] = (256 - checksum % 256) % 256

        return bytes(cmd_list)

    @staticmethod
    def checksum_valid(cmd: bytes) -> bool:
        """Checks if the checksum of the command is valid."""
        # Make sure current checksum is 0
        cmd_list = list(cmd)
        checksum = 0
        for x in cmd_list:
            checksum += x

        return checksum % 256 == 0

    @staticmethod
    def pack_command(cmd_id: int, cmd_ver: int, payload: bytes = b"") -> bytes:
        """Packs a command."""
        cmd_header = struct.pack(
            REQUEST_HEADER_FMT, 3, 0, cmd_id, cmd_ver, 0, len(payload)
        )

        cmd = cmd_header + payload
        cmd = HostCommand.update_checksum(cmd)

        return cmd

    def _send_request(self, comm) -> int:
        """Sends the command."""
        bytes_sent = comm.send(self.cmd_bytes)
        if bytes_sent != len(self.cmd_bytes):
            raise HostCommandError(f"Failed to send: {bytes_sent}")
        return bytes_sent

    def _receive_response(self, comm) -> tuple[bytes, ResponseHeaderType]:
        """Waits for and receives the response."""
        comm.wait()
        response_bytes = comm.receive()
        if len(response_bytes) < RESPONSE_HEADER_LEN:
            raise HostCommandError(
                f"Response shorther than header: {len(response_bytes)}"
            )
        try:
            response_header = ResponseHeaderType(
                *struct.unpack(
                    RESPONSE_HEADER_FMT, response_bytes[:RESPONSE_HEADER_LEN]
                )
            )
        except struct.error as e:
            raise HostCommandError(f"Failed to unpack header: {e}") from e

        remaining_bytes = response_header.data_length - (
            len(response_bytes) - RESPONSE_HEADER_LEN
        )
        if remaining_bytes > 0:
            response_bytes += comm.receive(remaining_bytes)
        return response_bytes, response_header

    def _validate_response(
        self, response_bytes, response_header
    ) -> EcCommandResult:
        """Validates the response."""

        expected_response_len = (
            struct.calcsize(self.response_fmt) + RESPONSE_HEADER_LEN
        )
        if self.variable_response:
            if expected_response_len > len(response_bytes):
                raise HostCommandError(
                    f"Variable response to short: {len(response_bytes)}"
                )
            expected_response_len = len(response_bytes)

        if expected_response_len != len(response_bytes):
            raise HostCommandError(
                "Invalid response length. Received: "
                + str(len(response_bytes))
                + " expected: "
                + str(expected_response_len)
            )

        if not HostCommand.checksum_valid(response_bytes):
            raise HostCommandError("Response checksum invalid")

        if response_header.result != EcCommandResult.SUCCESS:
            return EcCommandResult(response_header.result)

        return EcCommandResult.SUCCESS

    def _adjust_format(self, response_len):
        if self.variable_response:
            remaining_variable_payload = (
                response_len
                - RESPONSE_HEADER_LEN
                - struct.calcsize(self.response_fmt)
            )
            self.response_fmt += f"{remaining_variable_payload}s"

    def _unpack_response(self, response_bytes):
        """Unpacks the response."""
        try:
            if self.response_type is not None:
                response_unnamed = struct.unpack(
                    self.response_fmt, response_bytes[RESPONSE_HEADER_LEN:]
                )
                self.response = self.response_type(*response_unnamed)
        except struct.error as e:
            raise HostCommandError("Failed to unpack response payload") from e

    def send_cmd(self, comm) -> EcCommandResult:
        """Sends the command."""
        try:
            self._send_request(comm)
            response_bytes, response_header = self._receive_response(comm)

            valid = self._validate_response(response_bytes, response_header)
            if valid != EcCommandResult.SUCCESS:
                return valid

            self._adjust_format(len(response_bytes))

            self._unpack_response(response_bytes)

        except HostCommandError as e:
            print(f"ERROR: {e}")
            return EcCommandResult.UNKNOWN
        except IOError as e:
            print(f"Communication error: {e}")
            return EcCommandResult.UNKNOWN

        return EcCommandResult.SUCCESS

    def process_payload(self, payload_fmt, payload_type):
        """Processes a variable-length payload."""
        payload = self.response[-1]
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
            **{self.response._fields[-1]: new_payload_field}
        )

    def run(self, comm) -> int:
        """Runs the host command."""
        return self.send_cmd(comm)


class GetVersionCmd(HostCommand):
    """Gets the version of the EC."""

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
    """Gets the supported versions of a command."""

    def __init__(self, command):
        # 4 bytes of version mask
        self.response_fmt = "<I"
        self.response_type = namedtuple("Response", ["version_mask"])
        # 2 bytes of cmd
        payload = struct.pack("<H", command)
        super().__init__(0x0008, 1, payload=payload)


class FlashInfoCmd(HostCommand):
    """Gets flash information."""

    def __init__(self, num_banks_desc: int):
        # 4 bytes of flash_size
        # 4 bytes of flags
        # 4 bytes of write_ideal_size
        # 2 bytes of num_banks_total
        # 2 bytes of num_banks_desc
        # num_banks_desc of ec_flash_bank struct
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
        super().__init__(0x0010, 2, payload=payload, variable_response=True)

    def run(self, comm) -> int:
        """Runs the host command."""
        ret = super().run(comm)
        if ret == EcCommandResult.SUCCESS:
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
            self.process_payload(ec_flash_bank_fmt, ec_flash_bank_type)
        return ret


class ProtocolInfoCmd(HostCommand):
    """Gets protocol information."""

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
    """Sets the FP mode."""

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
    """Returns the correct FpInfoCmd based on the supported versions."""
    versions = get_versions(0x0403)
    if 2 in versions:
        return FpInfoCmd2()
    if 1 in versions:
        return FpInfoCmd1()
    return None


class FpInfoCmd1(HostCommand):
    """Gets FP information (version 1)."""

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
    """Gets FP information (version 2)."""

    def __init__(self):
        super().__init__(0x0403, 2, variable_response=True)
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
        """Runs the host command."""
        ret = super().run(comm)
        if ret == EcCommandResult.SUCCESS:
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
            self.process_payload(image_frame_fmt, image_frame_type)
        return ret


class FpVendorCmd(HostCommand):
    """FP vendor command."""

    def __init__(self, param1: int):
        # Variable number of bytes in response
        self.response_fmt = "<"
        self.response_type = namedtuple(
            "Response",
            [
                "payload",
            ],
        )
        # 4 bytes of param1
        payload = struct.pack("<I", param1)
        super().__init__(0x040B, 0, payload=payload, variable_response=True)


class EnterBootloaderCmd(HostCommand):
    """Enters the bootloader."""

    def __init__(self):
        super().__init__(0x00E2)


class FlashReadCmd(HostCommand):
    """Reads from flash."""

    def __init__(self, offset: int, size: int):
        # "size" bytes of data

        self.response_fmt = f"{size}s"
        self.response_type = namedtuple("Response", ["data"])

        # 4 bytes of offset
        # 4 bytes of size
        payload = struct.pack("<II", offset, size)

        super().__init__(0x0011, 0, payload=payload)


class FlashWriteCmd(HostCommand):
    """Writes to flash."""

    def __init__(self, offset: int, data: bytes):
        size = len(data)
        # 4 bytes of offset
        # 4 bytes of size
        payload = struct.pack(f"<II{size}s", offset, size, data)

        super().__init__(0x0012, 0, payload=payload)


class FlashEraseCmd(HostCommand):
    """Erases flash."""

    def __init__(self, offset: int, size: int):
        # 4 bytes of offset
        # 4 bytes of size
        payload = struct.pack("<II", offset, size)

        super().__init__(0x0013, 0, payload=payload)


class FlashRegionInfoCmd(HostCommand):
    """Gets flash region information."""

    def __init__(self, region: int):
        # 4 bytes of region
        payload = struct.pack("<I", region)

        # 4 bytes of offset
        # 4 bytes of size
        self.response_fmt = "<II"
        self.response_type = namedtuple("Response", ["offset", "size"])
        super().__init__(0x0016, 1, payload=payload)


class RebootECCmd(HostCommand):
    """Reboots the EC."""

    def __init__(self, cmd: int, flags: int = 0):
        # 1 byte of command
        # 1 byte of flags
        payload = struct.pack("<BB", cmd, flags)

        super().__init__(0x00D2, 0, payload=payload)

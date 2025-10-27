# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for EC host commands."""

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
RESPONSE_HEADER_FMT = "<BBHHH"
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

    cmd_version: int = 0
    _cmd_bytes: bytes
    _request_fmt: str = ""
    _response_fmt: str = ""
    _response_type: type = None
    _variable_response: bool = False
    _variable_payload_fmt: str = ""
    _variable_payload_type: type = None

    def __init__(
        self,
        cmd_id,
        cmd_version=0,
        request_msg: list[tuple[type, str]] = None,
        response_msg: list[tuple[str, str]] = None,
        variable_payload_msg: list[tuple[str, str]] = None,
    ):
        """Initializes the host command."""
        self.cmd_version = cmd_version

        request_payload = b""
        if request_msg:
            _request_fmt = HostCommand._join_formats(request_msg)
            request_values = [value for value, _ in request_msg]
            request_payload = struct.pack(_request_fmt, *request_values)

        self._cmd_bytes = HostCommand.pack_command(
            cmd_id, cmd_version, request_payload
        )

        if response_msg:
            self._response_fmt = HostCommand._join_formats(response_msg)
            self._response_type = HostCommand._build_response(response_msg)
            self._variable_response = False
            # last field without known length means variable length response
            _, last_field_fmt = response_msg[-1]
            self._variable_response = last_field_fmt == ""

        if variable_payload_msg:
            self._variable_payload_fmt = HostCommand._join_formats(
                variable_payload_msg
            )
            self._variable_payload_type = HostCommand._build_response(
                variable_payload_msg
            )

        self.response = None

    @staticmethod
    def update_checksum(cmd: bytes) -> bytes:
        """Updates the checksum of the command."""
        # Make sure current checksum is 0
        assert len(cmd) >= REQUEST_HEADER_LEN, "Too Short request"
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

    @staticmethod
    def _join_formats(msg) -> str:
        """Joins formats to create format string."""
        return "<" + "".join(format for _, format in msg)

    @staticmethod
    def _build_response(msg) -> type:
        """Builds response namedtuple."""
        return namedtuple("Response", [name for name, _ in msg])

    def _send_request(self, comm) -> int:
        """Sends the command."""
        bytes_sent = comm.send(self._cmd_bytes)
        if bytes_sent != len(self._cmd_bytes):
            raise HostCommandError(f"Failed to send: {bytes_sent}")
        return bytes_sent

    def _receive_response(self, comm) -> tuple[bytes, ResponseHeaderType]:
        """Waits for and receives the response."""
        comm.wait()
        response_bytes = comm.receive()
        if len(response_bytes) < RESPONSE_HEADER_LEN:
            raise HostCommandError(
                f"Response shorter than header: {len(response_bytes)}"
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
            struct.calcsize(self._response_fmt) + RESPONSE_HEADER_LEN
        )
        if self._variable_response:
            if expected_response_len > len(response_bytes):
                raise HostCommandError(
                    f"Variable response too short: {len(response_bytes)}, "
                    f"expected at least: {expected_response_len}"
                )
            expected_response_len = len(response_bytes)

        if expected_response_len != len(response_bytes):
            raise HostCommandError(
                f"Invalid response length. Received: {len(response_bytes)}, "
                f"expected: {expected_response_len}"
            )

        if not HostCommand.checksum_valid(response_bytes):
            raise HostCommandError("Response checksum invalid")

        if response_header.result != EcCommandResult.SUCCESS:
            return EcCommandResult(response_header.result)

        return EcCommandResult.SUCCESS

    def _adjust_response_fmt(self, response_len):
        if self._variable_response:
            remaining_variable_payload = (
                response_len
                - RESPONSE_HEADER_LEN
                - struct.calcsize(self._response_fmt)
            )
            self._response_fmt += f"{remaining_variable_payload}s"

    def _unpack_response(self, response_bytes):
        """Unpacks the response."""
        try:
            if self._response_type:
                response_unnamed = struct.unpack(
                    self._response_fmt, response_bytes[RESPONSE_HEADER_LEN:]
                )
                self.response = self._response_type(*response_unnamed)
        except struct.error as e:
            raise HostCommandError("Failed to unpack response payload") from e

    def _process_variable_payload(self):
        """Processes a variable-length payload."""
        if not self._variable_payload_type:
            return
        payload = self.response[-1]  # process last field
        element_len = struct.calcsize(self._variable_payload_fmt)
        element_nr = int(len(payload) / element_len)
        new_payload_field = [
            self._variable_payload_type(
                *struct.unpack_from(
                    self._variable_payload_fmt,
                    payload,
                    i * element_len,
                )
            )
            for i in range(element_nr)
        ]
        self.response = self.response._replace(
            **{self.response._fields[-1]: new_payload_field}
        )

    def send_cmd(self, comm) -> EcCommandResult:
        """Sends the command."""
        try:
            self._send_request(comm)
            response_bytes, response_header = self._receive_response(comm)

            valid = self._validate_response(response_bytes, response_header)
            if valid != EcCommandResult.SUCCESS:
                return valid

            self._adjust_response_fmt(len(response_bytes))

            self._unpack_response(response_bytes)

            self._process_variable_payload()

        except HostCommandError as e:
            print(f"ERROR: {e}")
            return EcCommandResult.UNKNOWN
        except IOError as e:
            print(f"Communication error: {e}")
            return EcCommandResult.UNKNOWN

        return EcCommandResult.SUCCESS

    def run(self, comm) -> int:
        """Runs the host command."""
        return self.send_cmd(comm)


class GetVersionCmd1(HostCommand):
    """Gets the version of the EC."""

    def __init__(self):
        # 32 bytes of RO string version
        # 32 bytes of RW string version
        # 32 bytes of fwid_ro
        # 4 bytes of current image
        # 32 bytes of fwid_rw
        response_msg = [
            ("ro_ver", "32s"),
            ("rw_ver", "32s"),
            ("fwid_ro", "32s"),
            ("curr_image", "I"),
            ("fwid_rw", "32s"),
        ]
        super().__init__(0x0002, 1, response_msg=response_msg)


class GetVersionsCmd1(HostCommand):
    """Gets the supported versions of a command."""

    def __init__(self, command):
        # 4 bytes of version mask
        response_msg = [("version_mask", "I")]
        # 2 bytes of cmd
        request_msg = [(command, "H")]
        super().__init__(
            0x0008, 1, response_msg=response_msg, request_msg=request_msg
        )


class FlashInfoCmd2(HostCommand):
    """Gets flash information."""

    def __init__(self, num_banks_desc: int):
        # 4 bytes of flash_size
        # 4 bytes of flags
        # 4 bytes of write_ideal_size
        # 2 bytes of num_banks_total
        # 2 bytes of num_banks_desc
        # num_banks_desc of ec_flash_bank struct
        response_msg = [
            ("flash_size", "I"),
            ("flags", "I"),
            ("write_ideal_size", "I"),
            ("num_banks_total", "H"),
            ("num_banks_desc", "H"),
            ("ec_flash_banks", ""),
        ]
        # 2 bytes of num_banks
        # 2 bytes reserved
        request_msg = [(num_banks_desc, "H"), (0, "H")]
        # 2 bytes of count
        # 1 byte of size_exp
        # 1 byte of write_size_exp
        # 1 byte of erase_size_exp
        # 1 byte of protect_size_exp
        # 2 bytes of reserved
        ec_flash_bank = [
            ("count", "H"),
            ("size_exp", "B"),
            ("write_size_exp", "B"),
            ("erase_size_exp", "B"),
            ("protect_size_exp", "B"),
            ("reserved", "H"),
        ]
        super().__init__(
            0x0010,
            2,
            response_msg=response_msg,
            request_msg=request_msg,
            variable_payload_msg=ec_flash_bank,
        )


class ProtocolInfoCmd0(HostCommand):
    """Gets protocol information."""

    def __init__(self):
        # 4 bytes of protocol version
        # 2 bytes of max_request_packet_size
        # 2 bytes of max_response_packet_size
        # 4 bytes of flags
        response_msg = [
            ("protocol_versions", "I"),
            ("max_request_packet_size", "H"),
            ("max_response_packet_size", "H"),
            ("flags", "I"),
        ]
        super().__init__(0x000B, response_msg=response_msg)


class FpModeCmd0(HostCommand):
    """Sets the FP mode."""

    def __init__(self, mode: int):
        # 4 bytes of mode
        response_msg = [("mode", "I")]
        # 4 bytes of param1
        request_msg = [(mode, "I")]
        super().__init__(
            0x0402, 0, response_msg=response_msg, request_msg=request_msg
        )


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
        response_msg = [
            ("vendor_id", "I"),
            ("product_id", "I"),
            ("model_id", "I"),
            ("version", "I"),
            ("frame_size", "I"),
            ("pixel_format", "I"),
            ("width", "H"),
            ("height", "H"),
            ("bpp", "H"),
            ("errors", "H"),
            ("template_size", "I"),
            ("template_max", "H"),
            ("template_valid", "H"),
            ("template_dirty", "I"),
            ("template_version", "I"),
        ]
        super().__init__(0x0403, 1, response_msg=response_msg)


class FpInfoCmd2(HostCommand):
    """Gets FP information (version 2)."""

    def __init__(self):
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
        response_msg = [
            ("vendor_id", "I"),
            ("product_id", "I"),
            ("model_id", "I"),
            ("version", "I"),
            ("num_capture_types", "H"),
            ("errors", "H"),
            ("template_size", "I"),
            ("template_max", "H"),
            ("template_valid", "H"),
            ("template_dirty", "I"),
            ("template_version", "I"),
            ("image_frame_params", ""),
        ]
        # fp_image_frame_params
        # 4 bytes of frame_size;
        # 4 bytes of pixel_format;
        # 2 bytes of width;
        # 2 bytes of height;
        # 2 bytes of bpp;
        # 1 byte of fp_capture_type;
        # 1 byte of reserved;
        image_frame = [
            ("frame_size", "I"),
            ("pixel_format", "I"),
            ("width", "H"),
            ("height", "H"),
            ("bpp", "H"),
            ("fp_capture_type", "B"),
            ("reserved", "B"),
        ]
        super().__init__(
            0x0403,
            2,
            response_msg=response_msg,
            variable_payload_msg=image_frame,
        )


class FpVendorCmd0(HostCommand):
    """FP vendor command."""

    def __init__(self, param1: int):
        # Variable number of bytes in response
        response_msg = [("payload", "")]
        # 4 bytes of param1
        request_msg = [(param1, "I")]
        super().__init__(
            0x040B, 0, response_msg=response_msg, request_msg=request_msg
        )


class EnterBootloaderCmd0(HostCommand):
    """Enters the bootloader."""

    def __init__(self):
        super().__init__(0x00E2)


class FlashReadCmd0(HostCommand):
    """Reads from flash."""

    def __init__(self, offset: int, size: int):
        # "size" bytes of data
        response_msg = [("data", f"{size}s")]
        # 4 bytes of offset
        # 4 bytes of size
        request_msg = [(offset, "I"), (size, "I")]
        super().__init__(
            0x0011, 0, response_msg=response_msg, request_msg=request_msg
        )


class FlashWriteCmd0(HostCommand):
    """Writes to flash."""

    def __init__(self, offset: int, data: bytes):
        size = len(data)
        # 4 bytes of offset
        # 4 bytes of size
        request_msg = [(offset, "I"), (size, "I"), (data, f"{size}s")]

        super().__init__(0x0012, 0, request_msg=request_msg)


class FlashEraseCmd0(HostCommand):
    """Erases flash."""

    def __init__(self, offset: int, size: int):
        # 4 bytes of offset
        # 4 bytes of size
        request_msg = [(offset, "I"), (size, "I")]

        super().__init__(0x0013, 0, request_msg=request_msg)


class FlashRegionInfoCmd1(HostCommand):
    """Gets flash region information."""

    def __init__(self, region: int):
        # 4 bytes of offset
        # 4 bytes of size
        response_msg = [("offset", "I"), ("size", "I")]
        # 4 bytes of region
        request_msg = [(region, "I")]
        super().__init__(
            0x0016, 1, response_msg=response_msg, request_msg=request_msg
        )


class RebootECCmd0(HostCommand):
    """Reboots the EC."""

    def __init__(self, cmd: int, flags: int = 0):
        # 1 byte of command
        # 1 byte of flags
        request_msg = [(cmd, "B"), (flags, "B")]

        super().__init__(0x00D2, 0, request_msg=request_msg)

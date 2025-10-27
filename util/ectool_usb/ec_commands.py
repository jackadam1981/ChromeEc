# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for EC host command definitions."""

from enum import IntEnum

from command import HostCommand


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


class ECCommand(HostCommand):
    """Runs the EC command."""

    def run(self, comm) -> EcCommandResult:
        ret = super().run(comm)
        try:
            return EcCommandResult(ret)
        except ValueError:
            print(f"Unknown error code {ret} returned by EC.")
            return EcCommandResult.UNKNOWN


class GetVersionCmd1(ECCommand):
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


class GetVersionsCmd1(ECCommand):
    """Gets the supported versions of a command."""

    def __init__(self, command):
        # 4 bytes of version mask
        response_msg = [("version_mask", "I")]
        # 2 bytes of cmd
        request_msg = [(command, "H")]
        super().__init__(
            0x0008, 1, response_msg=response_msg, request_msg=request_msg
        )


class FlashInfoCmd2(ECCommand):
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


class ProtocolInfoCmd0(ECCommand):
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
        super().__init__(0x000B, 0, response_msg=response_msg)


class FpModeCmd0(ECCommand):
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


class FpInfoCmd1(ECCommand):
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


class FpInfoCmd2(ECCommand):
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


class FpVendorCmd0(ECCommand):
    """FP vendor command."""

    def __init__(self, param1: int):
        # Variable number of bytes in response
        response_msg = [("payload", "")]
        # 4 bytes of param1
        request_msg = [(param1, "I")]
        super().__init__(
            0x040B, 0, response_msg=response_msg, request_msg=request_msg
        )


class EnterBootloaderCmd0(ECCommand):
    """Enters the bootloader."""

    def __init__(self):
        super().__init__(0x00E2, 0)


class FlashReadCmd0(ECCommand):
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


class FlashWriteCmd0(ECCommand):
    """Writes to flash."""

    def __init__(self, offset: int, data: bytes):
        size = len(data)
        # 4 bytes of offset
        # 4 bytes of size
        request_msg = [(offset, "I"), (size, "I"), (data, f"{size}s")]

        super().__init__(0x0012, 0, request_msg=request_msg)


class FlashEraseCmd0(ECCommand):
    """Erases flash."""

    def __init__(self, offset: int, size: int):
        # 4 bytes of offset
        # 4 bytes of size
        request_msg = [(offset, "I"), (size, "I")]

        super().__init__(0x0013, 0, request_msg=request_msg)


class FlashRegionInfoCmd1(ECCommand):
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


class RebootECCmd0(ECCommand):
    """Reboots the EC."""

    def __init__(self, cmd: int, flags: int = 0):
        # 1 byte of command
        # 1 byte of flags
        request_msg = [(cmd, "B"), (flags, "B")]

        super().__init__(0x00D2, 0, request_msg=request_msg)

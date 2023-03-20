#!/usr/bin/env python3
"""
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

The background how to apply crisis to update internal flash, e.g. via Servo:
1: Servo holds EC in reset state;
2: Servo asserts UART crisis strap pin;
3: Servo releases EC out of reset state;
4: EC Rom loader enters crisis mode;
5: Servo downloads 2nd loader into EC Sram via UART;
 - protocol is fixed per ROM specification;
6: After download 2nd loader into SRAM, ROM will validate 2nd loader image,
   then jump to execute 2nd loader if passing validation;
7: Servo again communicates with 2nd loader in SRAM
   to update internal flash via UART; - protocol is flexible;
8: after flash is done, Servo de-asserts UART crisis strap pin,
   and generate a low pulse to reset EC via nRESET_IN;
9: EC Rom loader loads updated FW image in internal flash;
 """
import argparse
import binascii
import logging
import os
import struct
import sys
import time

import serial


CRC_LENGTH = 4

HEADER_WRITE_CMD_ID = 0x65
KEY_HASH_BLOB_WRITE_CMD_ID = 0x66
FW_IMAGE_WRITE_CMD_ID = 0x67
GET_FW_INFO_CMD_ID = 0x70
SRAM_EXE_CMD_ID = 0x69
EC_RESPONSE_BYTES_COUNT = 262272
IMAGE_CHUNK_LENGTH = 128
HEADER_CHUNK_LENGTH = 64
HEADER_LENGTH = 320
START_OFFSET = 0
ACK_TIMEOUT_SECS = 22
RESPONSE_LENGTH = 5
RETRY_COUNT = 3
DELAY_50_MS = 0.05

HOST_NEGO_CMD_SEQ = [0x55, 0xAA]
HOST_NEGO_CMD_RESP_SEQ = [0x5A, 0xA5]
FLASH_PGM_CMD_SEQ = [0x33, 0xCC]
FLASH_PGM_CMD_RESP_SEQ = [0x3C, 0xC3]

VENDOR_ID = b"MCHP"
PGM_HEADER = b"\x01\x08"
PRG_HEADER_LEN = b"\x08"
TERMINATOR_ID = b"XEOF"
PRG_HDR_TOTAL_LEN = 0xF0

MCHP_VENDOR_ID_START = 0
MCHP_VENDOR_ID_END = len(VENDOR_ID)
MCHP_PGM_HDR_START = 6
MCHP_PGM_HDR_END = MCHP_PGM_HDR_START + len(PGM_HEADER)
MCHP_PGM_HDR_LEN_START = 13
MCHP_PGM_HDR_LEN_END = MCHP_PGM_HDR_LEN_START + len(PRG_HEADER_LEN)
MCHP_TERM_ID_START = PRG_HDR_TOTAL_LEN - 8
MCHP_TERM_ID_END = MCHP_TERM_ID_START + len(TERMINATOR_ID)


logging.basicConfig(level=logging.INFO)


def update_progress(curr_val, max_val):
    """Updates the header/firmware transfer status in console.

    Args:
        curr_val: Size currently transferred
        max_val: Maximum size needs to be transferred
    """
    print(f"Transfer {curr_val} of {max_val}.", end="\r")


def response_bytes(command_id):
    """Returns the response bytes for the FW IMAGE WRITE CMD.

    Args:
        command_id: Given command ID

    Returns:
        resp_bytes: Response byte for the given command ID
    """
    cmd = CommandResponse(command_id)
    cmd_bytes = cmd.bytes()
    resp_bytes = []
    for items in cmd_bytes:
        items = struct.pack("B", items)
        resp_bytes.append(items)
    return resp_bytes


def ec_payload_transfer(uart_handle, ec_file):
    """Transfer the EC payload into the internal flash.

    Args:
        uart_handle: UART handle
        ec_file: EC file path in binary format
    """
    resp_bytes = response_bytes(FW_IMAGE_WRITE_CMD_ID)
    with open(ec_file, mode="rb") as data:
        image = data.read()

    cmd = CommandPayload(FW_IMAGE_WRITE_CMD_ID)
    offset = START_OFFSET
    retry_count = RETRY_COUNT

    logging.info("EC FW image transfer in progress")
    max_len = os.path.getsize(ec_file)
    while offset < max_len:
        update_progress(offset, max_len)
        file_content = image[offset : offset + IMAGE_CHUNK_LENGTH]
        cmd.payload_offset = offset
        cmd.data_bytes = file_content
        cmd.payload_length = len(file_content)
        cmd_bytes = cmd.bytes()

        uart_handle.write(cmd_bytes)

        uart_handle.reset_input_buffer()
        if (
            offset == EC_RESPONSE_BYTES_COUNT
            or (os.path.getsize(ec_file) - offset) == IMAGE_CHUNK_LENGTH
        ):
            logging.info(
                "Waiting for EC to program. Will take about %ss.",
                ACK_TIMEOUT_SECS,
            )
            time.sleep(ACK_TIMEOUT_SECS)
        rf_cont_disp = []

        for _ in range(RESPONSE_LENGTH):
            read_data = uart_handle.read(1)
            rf_cont_disp.append(read_data)

        if rf_cont_disp != resp_bytes:
            retry_count = retry_count - 1
            if retry_count == 0:
                logging.critical("Timeout Error - Response pattern not match")
                logging.critical("Expected Response - %s", resp_bytes)
                logging.critical("Received Response - %s", rf_cont_disp)
                sys.exit(1)
            logging.info("Response pattern not matching ,retrying for 3 times")
            continue

        offset = offset + len(file_content)

    logging.info("EC FW image transfer completed")


def ec_header_transfer(uart_handle):
    """Transfer the EC header binary into the internal flash.

    Args:
        uart_handle: Handle of the UART port
    """

    header_gen = bytearray(PRG_HDR_TOTAL_LEN)

    header_gen[MCHP_VENDOR_ID_START:MCHP_VENDOR_ID_END] = VENDOR_ID
    header_gen[MCHP_PGM_HDR_START:MCHP_PGM_HDR_END] = PGM_HEADER
    header_gen[MCHP_PGM_HDR_LEN_START:MCHP_PGM_HDR_LEN_END] = PRG_HEADER_LEN
    header_gen[MCHP_TERM_ID_START:MCHP_TERM_ID_END] = TERMINATOR_ID

    logging.info("EC program header transfer in progress")
    transfer(
        uart_handle,
        HEADER_WRITE_CMD_ID,
        PRG_HDR_TOTAL_LEN,
        PRG_HDR_TOTAL_LEN,
        header_gen,
    )
    logging.info("EC program header transfer completed")


def transfer(
    uart_handle, command_id, max_write_length, payload_length, payload
):
    """Transfer the payload with the command sequence.

    Args:
        uart_handle: Handle of the UART port
        command_id: Given command ID
        max_write_length: Maximum number of bytes written to the UART
                          during a single write
        payload_length: Total length of the payload
        payload: Payload bytes to write
    """

    resp_bytes = response_bytes(command_id)
    # command Framing of the given payloads
    cmd = CommandPayload(command_id)
    offset = START_OFFSET
    retry_count = RETRY_COUNT
    while offset < payload_length:
        update_progress(offset, payload_length)
        cmd.payload_offset = offset
        cmd.payload_length = len(payload[offset : offset + max_write_length])
        cmd.data_bytes = payload[offset : offset + max_write_length]
        cmd_bytes = cmd.bytes()

        uart_handle.write(cmd_bytes)

        time.sleep(DELAY_50_MS)  # wait for EC to respond
        uart_handle.reset_input_buffer()
        rf_cont_disp = []

        for _ in range(RESPONSE_LENGTH):
            read_data = uart_handle.read(1)
            rf_cont_disp.append(read_data)

        if rf_cont_disp != resp_bytes:
            logging.info("For the Header file transferring")
            retry_count = retry_count - 1
            if retry_count == 0:
                logging.critical("Timeout Error - Response pattern not match")
                logging.critical("Expected Response - %s", resp_bytes)
                logging.critical("Received Response - %s", rf_cont_disp)
                sys.exit(1)
            logging.info("Response pattern not matching ,retrying for 3 times")
            continue

        offset = offset + cmd.payload_length


def payload_transfer(uart_handle, spi_file):
    """Transfer the second loader image into the SRAM to execute.

    Args:
        uart_handle: Handle of the UART port
        spi_file: Given SPI image file
    """
    tag_base_addr = 0
    with open(spi_file, mode="rb") as data:
        image = data.read()

    # Extracting the content
    addr = (struct.unpack("<I", image[0:4]))[0]
    addr &= 0xFFFFFF
    if addr != 0xFFFFFF:
        tag_base_addr = addr << 8

    hdr_addr = tag_base_addr
    header_len = HEADER_LENGTH
    img_offset = hdr_addr + header_len
    hdr_file_data = image[tag_base_addr : tag_base_addr + header_len]
    encryption_enable = struct.unpack("B", hdr_file_data[0x06:0x07])[0]
    img_len = (
        struct.unpack("<H", hdr_file_data[0x10:0x12])[0]
    ) * IMAGE_CHUNK_LENGTH
    if encryption_enable == 0x80:  # Encryption
        img_len = int(img_len) + 512 - 16
    else:
        img_len = int(img_len) + 384 - 16  # Without Encryption

    logging.info("Secondary loader header transfer in progress")
    transfer(
        uart_handle,
        HEADER_WRITE_CMD_ID,
        HEADER_CHUNK_LENGTH,
        header_len,
        image[hdr_addr : hdr_addr + header_len],
    )
    logging.info("Secondary loader header transfer completed")

    logging.info("Secondary loader FW image transfer in progress")
    transfer(
        uart_handle,
        FW_IMAGE_WRITE_CMD_ID,
        IMAGE_CHUNK_LENGTH,
        img_len,
        image[img_offset : img_offset + img_len],
    )
    logging.info("Secondary loader FW image transfer completed")


def get_crc_bytes(_bytes):
    """The below routine will calculate CRC for given

    Args:
        _bytes: Given bytes

    Returns:
        crc_bytes: integer to bytes of crc32
    """
    crc_data = binascii.crc32(_bytes)
    crc_bytes = convert_crc_int_to_bytes(crc_data)
    return crc_bytes


def convert_crc_int_to_bytes(integer_crc_data):
    """The below function will convert the integer crc to
        byte array format

    Args:
        integer_crc_data: integer value

    Returns:
        crc_bytes: computed crc for the given data
        e.g. Input: Integer crc = 0xb016f118 will return
        bytearray[4] = {0x18, 0xf1, 0x16, 0xb0}
    """
    crc_bytes = bytearray()
    for i in range(CRC_LENGTH):
        data = integer_crc_data & 0xFF
        crc_bytes.append(data)
        integer_crc_data = integer_crc_data >> 8
        i = i + 1
    return crc_bytes


class CommandIf:
    """Command Interface command"""

    # pylint: disable=too-few-public-methods
    index = None
    _cmd_bytes_sequence = None

    def __init__(self, cmd_id):
        self._cmd_bytes_sequence = bytearray()
        self.index = cmd_id

    def _get_crc(self):
        """Return crc bytes"""
        pec = get_crc_bytes(self._cmd_bytes_sequence)
        return pec


class CommandPayload(CommandIf):
    """Command Interface for payload command"""

    # pylint: disable=too-few-public-methods
    def __init__(self, command):
        CommandIf.__init__(self, command)
        self.payload_length = 0
        self.index = command
        self.payload_offset = 0
        self.data_bytes = bytearray()

    def __get_general_parameter_bytes(self):
        """Get parameter to initialize"""
        cmd_bytes = bytearray()
        cmd_bytes.append(self.index)
        cmd_bytes.append(self.payload_length)
        cmd_bytes.append(self.payload_offset & 0xFF)
        cmd_bytes.append((self.payload_offset >> 8) & 0xFF)
        cmd_bytes.append((self.payload_offset >> 16) & 0xFF)
        cmd_bytes.extend(self.data_bytes)
        return cmd_bytes

    def bytes(self):
        """Return bytes"""
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence = self.__get_general_parameter_bytes()
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


class CommandResponse(CommandIf):
    """Command Interface for response command"""

    # pylint: disable=too-few-public-methods
    def __init__(self, command):
        CommandIf.__init__(self, command)

    def bytes(self):
        """Return bytes"""
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(self.index)
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


class CommandGetfwinfo(CommandIf):
    """Command Interface for get response response command"""

    # pylint: disable=too-few-public-methods
    def __init__(self, command):
        CommandIf.__init__(self, GET_FW_INFO_CMD_ID)
        self.index = command

    def bytes(self):
        """Return bytes"""
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(self.index)
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


class CommandSramexe(CommandIf):
    """Command Interface for SRAM EXE command"""

    # pylint: disable=too-few-public-methods
    def __init__(self, command):
        CommandIf.__init__(self, SRAM_EXE_CMD_ID)
        self.index = command

    def bytes(self):
        """Return bytes"""
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(self.index)
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


def command_write(uart_handle, cmd_seq, resp_seq):
    """flash program command is used to establish communication

    Args:
        uart_handle: Handle of the UART port
        cmd_seq: Sequence of command bytes to transfer
        resp_seq: Expected response sequence

    Returns:
        Compares the received response against the expected one
        and returns True/False
    """
    uart_handle.reset_input_buffer()
    cmd_bytes = bytearray(cmd_seq)

    uart_handle.write(cmd_bytes)

    i = 0
    while i < 3:
        read = uart_handle.read(2)
        if len(read) == 0:
            i += 1
        else:
            if read[0] == resp_seq[0] and read[1] == resp_seq[1]:
                return True
            if read[0] == resp_seq[1] and read[1] == resp_seq[0]:
                return True

    # Retry for 3 times to check the communciation established
    return False


def sram_exec(uart_handle):
    """SRAM EXE command to BROM to run the second loader image in SRAM

    Args:
        uart_handle: Handle of the UART port
    """
    cmd = CommandResponse(SRAM_EXE_CMD_ID)
    cmd_bytes = cmd.bytes()
    resp_bytes = []
    for items in cmd_bytes:
        items = struct.pack("B", items)
        resp_bytes.append(items)
    cmd = CommandSramexe(SRAM_EXE_CMD_ID)
    cmd_bytes = cmd.bytes()

    uart_handle.write(cmd_bytes)

    uart_handle.reset_input_buffer()
    time.sleep(DELAY_50_MS)
    response_sram_exe = []

    for _ in range(RESPONSE_LENGTH):
        read = uart_handle.read(1)
        response_sram_exe.append(read)

    if resp_bytes == response_sram_exe:
        logging.info("Command to execute secondary loader image transferred")


def open_serial_port(port, baud):
    """Routine to open the comp port number handle.

    Args:
        port: Comport number
        baud: Given baud rate

    Returns:
        comport: UART port handle
    """
    comport = serial.Serial(port, baudrate=baud, stopbits=1, timeout=10)
    return comport


def main(args):
    """Crisis recovery utility routine for second loader into SRAM
    and program the ec image into the internal flash ."""
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument(
        "-s",
        "--seconday-loader-spi-image",
        dest="spi_file",
        type=str,
        required=False,
        help="secondary loader SPI image file",
    )
    parser.add_argument(
        "-p",
        "- com port path",
        dest="uart_port",
        type=str,
        required=False,
        help="Servo EC UART port number to be specified",
    )
    parser.add_argument(
        "-e",
        "- EC Image to flash",
        dest="ec_file",
        type=str,
        required=False,
        help="EC Image to be programmed in internal flash",
    )

    args = parser.parse_args()  # outputs

    if args.uart_port:
        comp_port = os.path.normpath(args.uart_port)
    else:
        logging.error("comp port not exist")
        sys.exit(1)

    if args.spi_file:
        if not os.path.exists(args.spi_file):
            logging.error("SPI File not exist")
            sys.exit(1)
        else:
            uart_handle = open_serial_port(comp_port, 9600)
            process = command_write(
                uart_handle, HOST_NEGO_CMD_SEQ, HOST_NEGO_CMD_RESP_SEQ
            )
            if process:
                payload_transfer(uart_handle, args.spi_file)
                uart_handle.reset_input_buffer()
                sram_exec(uart_handle)

            uart_handle.close()
            if process is False:
                logging.critical(
                    "Timeout Error - Did not receive response from EC."
                )
                sys.exit(1)

    if args.ec_file:
        if not os.path.exists(args.ec_file):
            logging.error("EC File not exist")
            sys.exit(1)
        else:
            uart_handle = open_serial_port(comp_port, 57600)

            process = command_write(
                uart_handle, FLASH_PGM_CMD_SEQ, FLASH_PGM_CMD_RESP_SEQ
            )
            if process:
                ec_header_transfer(uart_handle)
                ec_payload_transfer(uart_handle, args.ec_file)

            uart_handle.close()
            if process is False:
                logging.critical(
                    "Timeout Error - Did not receive response from EC."
                )
                sys.exit(1)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

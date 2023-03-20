#!/usr/bin/env python3
"""
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

The background how to apply crisis to update 
   internal flash, e.g. via Servo:
1: Servo holds EC in reset state;
2: Servo asserts UART crisis strap pin;
3: Servo releases EC out of reset state;
4: EC Rom loader enters crisis mode;
5: Servo downloads 2nd loader into EC Sram via UART;
 - protocol is fixed per ROM specification;
6: after download 2nd loader into SRAM, 
   Rom will validate 2nd loader image, 
then jump to execute 2nd loader if passing validation;
7: Servo again communicates with 2nd loader in SRAM 
   to update internal flash via UART; - protocol is flexible;
8: after flash is done, Servo de-asserts UART crisis strap pin,
   and generate a low pulse to reset EC via nRESET_IN;
9: EC Rom loader loads updated FW image in internal flash;
 """
import sys
import os
import argparse
import struct
import binascii
import time
import logging
import serial
from progress.bar import Bar

CRC_LENGTH = 4

HEADER_WRITE_CMD_ID = 0x65
KEY_HASH_BLOB_WRITE_CMD_ID = 0x66
FW_IMAGE_WRITE_CMD_ID = 0x67
GET_FW_INFO_CMD_ID = 0x70
SRAM_EXE_CMD_ID = 0x69

logging.basicConfig(level=logging.INFO)


def generate_pgm_header():
    """ Function to generate header file for the SPI image to be programmed"""
    mchp_header = struct.pack('BBBB', 0x4D, 0x43, 0x48, 0x50)
    pgm_header = struct.pack('BB', 0x01, 0x08)
    prg_header_len = struct.pack('B', 0x08)
    prg_header_data = struct.pack('BBBB', 0x58, 0x45, 0x4F, 0x46)
    prg_header_last_data = struct.pack('B', 0x00)
    prg_total_len = 0xF0
    header_gen = []
    with open("/tmp/flash_pgm_hdr.bin", "wb+") as header_gen:
        header_gen.seek(0)
        header_gen.write(mchp_header)
        header_gen.seek(6)
        header_gen.write(pgm_header)
        header_gen.seek(13)
        header_gen.write(prg_header_len)
        header_gen.seek(prg_total_len-8)
        header_gen.write(prg_header_data)
        header_gen.seek(prg_total_len-1)
        header_gen.write(prg_header_last_data)
        header_gen.close()


def response_bytes_fun(command_id):
    """ Function returns the response bytes for the FW IMAGE WRITE CMD"""
    # Image  Response data
    cmd_obj = CommandResponse(command_id)
    cmd_bytes = cmd_obj.bytes()
    response_bytes = []
    for items in cmd_bytes:
        items = struct.pack("B", items)
        response_bytes.append(items)
    return response_bytes


def ec_payload_transfer(uart_handle, ec_file):
    """ Function to transfer the ec payload into the INT flash """
    response_bytes = response_bytes_fun(FW_IMAGE_WRITE_CMD_ID)
    with open(ec_file, mode='rb') as data:
        image = data.read()

    data = image[0:0+os.path.getsize(ec_file)]
    cmd_obj = CommandPayload(FW_IMAGE_WRITE_CMD_ID)
    logging.info(
        "\n FW image File transferring for the internl flash programming \n")
    offset = 0
    retry_count = 3
    pbar = Bar("Processing ", max=os.path.getsize(ec_file))
    while offset < os.path.getsize(ec_file):
        file_content = data[offset:offset+128]
        cmd_obj.payload_offset = offset
        cmd_obj.data_bytes = file_content
        cmd_obj.payload_length = len(file_content)
        cmd_bytes = cmd_obj.bytes()
        for items in cmd_bytes:
            items = struct.pack('B', items)
            uart_handle.write(items)
        # time.sleep(0.05)  # wait for EC to respond
        uart_handle.reset_input_buffer()
        if offset == 262272 or (os.path.getsize(ec_file) - offset) == 128:
            logging.info(
                "\n*** Waiting for EC to program. Will take about 22s. ***")
            time.sleep(22)  # takes up to 22s to program before EC sends ACK
        rf_cont_disp = []
        i = 0
        while i < 5:
            read_data = uart_handle.read(1)
            rf_cont_disp.append(read_data)
            i = i + 1

        if rf_cont_disp != response_bytes:
            logging.info("Response pattern not matching ,retry for 3 times \n")
            logging.info(
                "For the EC FW Image transferring for the internl flash programming \n")
            retry_count = retry_count - 1
            if retry_count == 0:
                logging.info("Response pattern is not matching \n")
                logging.info(
                    "For the Header file transferring for the internl flash programming \n")
                break

        pbar.next()
        offset = offset+len(file_content)

    logging.info(
        "\n FW image File transferring for the internl flash programming completed \n")
    pbar.finish()


def ec_header_transfer(uart_handle):
    """ Function used to transfer the Flash header bin into the internal flash."""
    with open("/tmp/flash_pgm_hdr.bin", mode='rb') as data:
        image = data.read()

    logging.info(" Header updating into the internal Flash programming \n")
    transfer(uart_handle, HEADER_WRITE_CMD_ID, 240, 240, image)
    logging.info(
        "\n Header updating into the internal Flash programming  completed \n")

    logging.info(
        " EC_FW image updating into the internal Flash programming \n")


def transfer(uart_handle, command_id, pay_length, total_len, data):
    """Function used to transfer the payload with the command sequence."""
    # Response data for the command
    response_bytes = response_bytes_fun(command_id)
    # command Framing of the given payloads
    cmd_obj = CommandPayload(command_id)
    offset = 0
    retry_count = 3
    pbar = Bar("Processing ", fill='#', max=total_len)
    # print(" Header File transferring for the second loader image \n")
    while offset < total_len:
        cmd_obj.payload_offset = offset
        cmd_obj.payload_length = len(data[offset:offset+pay_length])
        cmd_obj.data_bytes = data[offset:offset+pay_length]
        cmd_bytes = cmd_obj.bytes()
        for items in cmd_bytes:
            items = struct.pack('B', items)
            uart_handle.write(items)
        time.sleep(0.05)  # wait for EC to respond
        uart_handle.reset_input_buffer()
        rf_cont_disp = []
        i = 0
        while i < 5:
            read_data = uart_handle.read(1)
            rf_cont_disp.append(read_data)
            i = i + 1

        if rf_cont_disp != response_bytes:
            logging.info("Response pattern not matching ,retry for 3 times \n")
            logging.info(
                "For the Header file transferring  \n")
            retry_count = retry_count - 1
            if retry_count == 0:
                logging.error("Response pattern is not matcing \n")
                logging.error(
                    "For the Header file transferring  \n")
                break

        pbar.next()
        offset = offset+len(data[offset:offset+pay_length])

    pbar.finish()


def payload_transfer(uart_handle, spi_file):
    """ Function to transfer the second loader image into the SRAM to execute."""
    batag0 = 0
    with open(spi_file, mode='rb') as data:
        image = data.read()

    # Extracting the content
    addr = struct.unpack("B", image[0:0+1])[0]
    addr |= struct.unpack("B", image[0+1:0+2])[0] << 8
    addr |= struct.unpack("B", image[0+2:0+3])[0] << 16
    if addr != 0xFFFFFF:
        batag0 = addr << 8

    hdr_addr_0 = batag0
    header_len = 320
    img_offset_0 = hdr_addr_0 + header_len
    hdr_file_data = image[batag0:batag0+header_len]
    img_en_enable = struct.unpack("B", hdr_file_data[0x06:0x07])[0]
    img_len_0 = (struct.unpack(
        "<H", hdr_file_data[0x10:0x11]+hdr_file_data[0x11:0x12])[0]) * 128
    if img_en_enable == 0x80:  # Encryption
        img_len_0 = int(img_len_0) + 512 - 16
    else:
        img_len_0 = int(img_len_0) + 384 - 16  # Without Encryption

    logging.info(" Header File transferring for the second loader image \n")
    transfer(uart_handle, HEADER_WRITE_CMD_ID, 64, 320,
             image[hdr_addr_0:hdr_addr_0+header_len])
    logging.info(
        "\n Header File transferring for the second loader image  completed \n")

    logging.info(" FW image File transferring for the second loader image \n")
    transfer(uart_handle, FW_IMAGE_WRITE_CMD_ID, 128, img_len_0,
             image[img_offset_0:img_offset_0+img_len_0])
    logging.info(
        "\n FW image File transferring for the second loader image  completed \n")


def get_crc(_bytes):
    """The below function will calculate CRC for given bytes."""
    return __crc32(_bytes)


def __crc32(_bytes):
    """crc32 bytes return """
    return binascii.crc32(_bytes)


def get_crc_bytes(_bytes):
    """The below function will calculate CRC for given"""
    crc_data = get_crc(_bytes)
    crc_bytes = convert_crc_int_to_bytes(crc_data)
    return crc_bytes


def convert_crc_int_to_bytes(integer_crc_data):
    """The below function will convert the integer crc to
    # byte array format
    # Return Type :  bytearray
    # e.g. Input  : Integer crc = 0xb016f118 will return
    # Return : bytearray[4] = {0x18, 0xf1, 0x16, 0xb0}"""
    crc_bytes = bytearray()
    for i in range(CRC_LENGTH):
        data = integer_crc_data & 0xFF
        byte_conv = struct.pack('B', data)
        crc_bytes.extend(byte_conv)
        integer_crc_data = integer_crc_data >> 8
        i = i+1
    return crc_bytes


class FlashProgramCommand:  # pylint: disable=too-few-public-methods
    """Flash Program command for two bytes to frame """

    def __init__(self):
        self._cmd_bytes_sequence = bytearray()
        # self._command1 = command1
        # self._command2 = command2

    def bytes(self):
        """ Funtion to return bytes """
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(0x33)  # self._command1)
        self._cmd_bytes_sequence.append(0xCC)  # self._command1)
        return self._cmd_bytes_sequence


class HostNegotiateCommand:  # pylint: disable=too-few-public-methods
    """Host Negotiate command """

    def __init__(self):
        self._cmd_bytes_sequence = bytearray()

    def bytes(self):
        """ Funtion to return bytes """
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(0x55)
        self._cmd_bytes_sequence.append(0xAA)
        return self._cmd_bytes_sequence


class CommandIf:  # pylint: disable=too-few-public-methods
    """Command Interface command """
    index = None
    _cmd_bytes_sequence = None

    def __init__(self, cmd_id):
        self._cmd_bytes_sequence = bytearray()
        self.index = cmd_id

    def _get_crc(self):
        """ Return crc bytes """
        pec = get_crc_bytes(self._cmd_bytes_sequence)
        return pec

    def bytes(self):
        """ Funtion to return bytes """

    @ property
    def _id(self):
        """ Return Id """
        return self.index


class CommandPayload(CommandIf):  # pylint: disable=too-few-public-methods
    """Command Interface for payload command """

    def __init__(self, command):
        CommandIf.__init__(self, command)
        self.payload_length = 0
        self.index = command
        self.payload_offset = 0
        self.data_bytes = bytearray()

    def __get_general_parameter_bytes(self):
        """function to get parameter to intialize"""
        cmd_bytes = bytearray()
        cmd_bytes.append(self.index)
        cmd_bytes.append(self.payload_length)
        cmd_bytes.append(self.payload_offset & 0xFF)
        cmd_bytes.append((self.payload_offset >> 8) & 0xFF)
        cmd_bytes.append((self.payload_offset >> 16) & 0xFF)
        cmd_bytes.extend(self.data_bytes)
        return cmd_bytes

    def bytes(self):
        """ Funtion to return bytes """
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence = self.__get_general_parameter_bytes()
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


class CommandResponse(CommandIf):  # pylint: disable=too-few-public-methods
    """Command Interface for response command """

    def __init__(self, command):
        CommandIf.__init__(self,  command)

    def bytes(self):
        """ Funtion to return bytes """
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(self.index)
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


class CommandGetfwinfo(CommandIf):  # pylint: disable=too-few-public-methods
    """Command Interface for get response response command """

    def __init__(self, command):
        CommandIf.__init__(self, GET_FW_INFO_CMD_ID)
        self.index = command

    def bytes(self):
        """ Funtion to return bytes """
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(self.index)
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


class CommandSramexe(CommandIf):  # pylint: disable=too-few-public-methods
    """Command Interface for SRAM EXE command """

    def __init__(self, command):
        CommandIf.__init__(self, SRAM_EXE_CMD_ID)
        self.index = command

    def bytes(self):
        """ Funtion to return bytes """
        self._cmd_bytes_sequence = bytearray()
        self._cmd_bytes_sequence.append(self.index)
        self._cmd_bytes_sequence.extend(self._get_crc())
        return self._cmd_bytes_sequence


def flash_program_command(uart_handle):
    """ flash program command is used to establish communication """
    match_pat = False
    uart_handle.reset_input_buffer()
    cmd_bytes = FlashProgramCommand()  # ProgramCommand(0x33, 0xCC)
    cmd_bytes = cmd_bytes.bytes()
    for items in cmd_bytes:
        items = struct.pack('B', items)
        uart_handle.write(items)
    i = 0
    while i < 3:
        read = uart_handle.read(2)
        if len(read) == 0:
            i += 1
        else:
            if read[0] == 60 and read[1] == 195:
                logging.info("Response Pattern matchced")
                match_pat = True
                break
            if read[0] == 195 and read[1] == 60:
                logging.info("Response Pattern matchced")
                match_pat = True
                break

        if i == 3:  # Retry for 3 times to check the communciation established
            logging.info("Timeout Error - Did not receive response from EC.")
            sys.exit(1)

    return match_pat


def host_negotiate_command(uart_handle):
    """ Host negottiate command is used to establish communication """
    match_pat = False
    uart_handle.reset_input_buffer()
    cmd_bytes = HostNegotiateCommand()
    cmd_bytes = cmd_bytes.bytes()
    for items in cmd_bytes:
        items = struct.pack('B', items)
        uart_handle.write(items)
    i = 0
    while i < 3:
        read = uart_handle.read(2)
        if len(read) == 0:
            i += 1
        else:
            if read[0] == 165 and read[1] == 90:
                logging.info("Response Pattern matchced")
                match_pat = True
                break
            if read[0] == 90 and read[1] == 165:
                logging.info("Response Pattern matchced")
                match_pat = True
                break

        if i == 3:  # Retry for 3 times to check the communciation established
            logging.error("Timeout Error - Did not receive response from EC.")
            sys.exit(1)

    return match_pat


def sram_exec_fun(uart_handle):
    """SRAM EXE command to BROM to run the second loader image in SRAM"""
    # SRAM_EXE_CMD_ID  Response data
    cmd_obj = CommandResponse(SRAM_EXE_CMD_ID)
    cmd_bytes = cmd_obj.bytes()
    response_bytes = []
    for items in cmd_bytes:
        items = struct.pack("B", items)
        response_bytes.append(items)
    cmd_obj = CommandSramexe(SRAM_EXE_CMD_ID)
    cmd_bytes = cmd_obj.bytes()
    for items in cmd_bytes:
        items = struct.pack('B', items)
        uart_handle.write(items)  # wr

    uart_handle.reset_input_buffer()
    time.sleep(0.05)
    i = 0
    response_sram_exe = []
    while i < 5:
        read = uart_handle.read(1)
        response_sram_exe.append(read)
        i = i + 1
    if response_bytes == response_sram_exe:
        logging.info("\nSuccessful to get Response received from the EC \n")
        logging.info(
            "Command to trigger to execute the crisis recovery image completed \n")


def open_serial_port(number, baud):
    """Function to open the comp port number handle ."""
    port = number
    pos = port.find(':')
    port = port[pos+1:]
    comport = serial.Serial(port,
                         baudrate=baud, stopbits=1, timeout=10)
    return comport


def main():
    """Function Main ."""
    logging.info("crisis_rcvry_util.py utility  \n ")
    spi_file_flag = False
    comp_port_flag = False
    ec_file_flag = False
    process = False
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-s", "- seconday_loader spi_image", dest="spi_file", type=str,
                        required=False, help="Input secondary loader SPI image  file")
    parser.add_argument("-p", "- com port number ", dest="comp_port", type=str,
                        required=False, help="Comport number to be specified")
    parser.add_argument("-e", "- EC Image to flash", dest="ec_file", type=str,
                        required=False, help="Final EC Image into internal flash")

    args = parser.parse_args()  # outputs

    if args.spi_file:
        spi_file = os.path.normpath(args.spi_file)
        spi_file_flag = True
        if not os.path.exists(spi_file):
            spi_file_flag = False
            logging.info("SPI File not exist")

    if args.comp_port:
        comp_port = os.path.normpath(args.comp_port)
        comp_port_flag = True

    if args.ec_file:
        ec_file = os.path.normpath(args.ec_file)
        ec_file_flag = True
        if not os.path.exists(ec_file):
            ec_file_flag = False
            logging.info("EC File not exist ")

    if spi_file_flag is True and comp_port_flag is True:
        logging.info(
            "crisis_rcvry_util.py utility to run the secondary loader image into the SRAM \n ")
        uart_handle = open_serial_port(comp_port, 9600)
        process = host_negotiate_command(uart_handle)
        if process is True:
            logging.info("\nEstablished the connection between Host <-> EC\n")

            payload_transfer(uart_handle, spi_file)

            uart_handle.reset_input_buffer()

            sram_exec_fun(uart_handle)

    if ec_file_flag is True and comp_port_flag is True:
        logging.info(
            "crisis_rcvry_util.py utility to program the EC image into the INT flash  \n ")
        generate_pgm_header()

        uart_handle = open_serial_port(comp_port, 57600)

        process = flash_program_command(uart_handle)
        if process is True:
            ec_header_transfer(uart_handle)
            ec_payload_transfer(uart_handle, ec_file)

    uart_handle.close()


if __name__ == "__main__":
    main()

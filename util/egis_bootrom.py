#!/usr/bin/env python3

from sys import byteorder
import usb.core
import sys
import time
import argparse
import os
from pathlib import Path
from hashlib import sha256

EC_DIR = Path(os.path.dirname(os.path.realpath(__file__))).parent
ZEPHYR_ET171_FW = os.path.join(EC_DIR, "build/zephyr/et171/output/ec.bin")

EGIS_VID = 0x1c7a
BOOTROM_PID = 0x1002

BOOT_INFO = [0x10]
FLASH_INFO = [0x11]

SPI_REG_BASE = 0xf0b00000
SPI_REG_ID = SPI_REG_BASE + 0x0
SPI_REG_TFMAT = SPI_REG_BASE + 0x10
SPI_REG_WRCNT = SPI_REG_BASE + 0x18
SPI_REG_RDCNT = SPI_REG_BASE + 0x1C
SPI_REG_TCTRL = SPI_REG_BASE + 0x20
SPI_REG_CMD = SPI_REG_BASE + 0x24
SPI_REG_ADDR = SPI_REG_BASE + 0x28
SPI_REG_DATA = SPI_REG_BASE + 0x2c

TCTRL_TRNS_MODE_OFFSET = 24

TCTRL_ADDR_FMT_MSK = (1 << 28)
TCTRL_ADDR_EN_MSK = (1 << 29)
TCTRL_CMD_EN_MSK = (1 << 30)

TRNS_MODE_WRITE_ONLY = (1 << TCTRL_TRNS_MODE_OFFSET)
TRNS_MODE_READ_ONLY = (2 << TCTRL_TRNS_MODE_OFFSET)
TRNS_MODE_NONE_DATA = (7 << TCTRL_TRNS_MODE_OFFSET)

# Address 3 bytes, no data merge, 8bits data unit.
# TODO consider checking CPOL, CPHA before
TFMAT = 0x00020700

FLASH_WREN = 0x06
FLASH_WRSR = 0x01
FLASH_RDSR = 0x05

def send_command(dev, data, expected_resp_size = 2, timeout = 200):
    sc_header_size = 1 + 1 + 4 + 32
    secure_channel = True
    if data[0] < 0x20 or data[0] > 0xe0:
        secure_channel = False

    cmd = []
    if secure_channel is True:
        cmd += [0xF4, 0x00] + list(len(data).to_bytes(4, byteorder="little"))
        cmd += sha256(bytes(data)).digest() #SHA256
        expected_resp_size += sc_header_size

    cmd += data
    dev.write(0x02, cmd)
    ret = dev.read(0x81, expected_resp_size, timeout)

    if secure_channel is True:
        if ret[0] != 0 or ret[1] != 0 or len(ret) < sc_header_size:
            return None
        return ret[sc_header_size:]
    else:
        return ret


def read_reg(dev, addr):
    off = list((addr).to_bytes(4, byteorder="little"))
    size = 1
    # One 32-bit balue
    length = list(size.to_bytes(4, byteorder="little"))
    # Use flash write to write to memory-mapped registers
    cmd = [0x20] + off + [0x04] + length
    ret = send_command(dev, cmd, 6)

    if ret[0] != 0:
        print("Incorrect resp format")
        return
    if ret[1] != 0:
        print("Error status: " + str(ret[1]))
        return

    return ret[2:]


def write_reg(dev, addr, value):
    off = list((addr).to_bytes(4, byteorder="little"))
    size = 1
    # One 32-bit balue
    length = list(size.to_bytes(4, byteorder="little"))
    data = list(value.to_bytes(4, byteorder="little"))
    # Use flash write to write to memory-mapped registers
    cmd = [0x21] + off + [0x04] + length + data
    ret = send_command(dev, cmd)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp write")
        print(ret)
        return


def write_enable(dev):
    write_reg(dev, SPI_REG_TFMAT, 0)
    tctrl = TRNS_MODE_NONE_DATA | TCTRL_CMD_EN_MSK
    write_reg(dev, SPI_REG_TCTRL, tctrl)
    write_reg(dev, SPI_REG_TFMAT, TFMAT)
    write_reg(dev, SPI_REG_CMD, FLASH_WREN)
    time.sleep(0.01)


def get_sr1(dev):
    # One byte (len - 1)
    write_reg(dev, SPI_REG_RDCNT, 0)
    tctrl = TRNS_MODE_READ_ONLY | TCTRL_CMD_EN_MSK
    write_reg(dev, SPI_REG_TCTRL, tctrl)
    write_reg(dev, SPI_REG_TFMAT, TFMAT)
    write_reg(dev, SPI_REG_CMD, FLASH_RDSR)
    time.sleep(0.01)
    sr1 = read_reg(dev, SPI_REG_DATA)
    return sr1[0]


def set_sr1(dev, sr1):
    write_enable(dev)

    # One byte (len - 1)
    write_reg(dev, SPI_REG_WRCNT, 0)
    tctrl = TRNS_MODE_WRITE_ONLY | TCTRL_CMD_EN_MSK
    write_reg(dev, SPI_REG_TCTRL, tctrl)
    write_reg(dev, SPI_REG_TFMAT, TFMAT)
    write_reg(dev, SPI_REG_DATA, sr1)
    write_reg(dev, SPI_REG_CMD, FLASH_WRSR)
    time.sleep(0.01)

def enable_pwm(dev):
    # CTRL
    write_reg(dev, 0xf0400020, 4)
    # RELOAD
    write_reg(dev, 0xf0400024, 0x00070a21)
    # CHNEN
    write_reg(dev, 0xf040001c, 0x00000008)
    # MUX
    write_reg(dev, 0xf0e00100, 0x0c000004)

def set_sr12(dev, sr1):
    write_enable(dev)

    # One byte (len - 1)
    write_reg(dev, SPI_REG_WRCNT, 0)
    tctrl = TRNS_MODE_WRITE_ONLY | TCTRL_CMD_EN_MSK
    write_reg(dev, SPI_REG_TCTRL, tctrl)
    write_reg(dev, SPI_REG_TFMAT, TFMAT)
    write_reg(dev, SPI_REG_DATA, sr1)
    write_reg(dev, SPI_REG_CMD, FLASH_WRSR)
    time.sleep(0.01)


def bootroom_info(dev):

    ret = send_command(dev, BOOT_INFO, 14)

    print("Bootrom info")
    print(ret)
    print("ver")
    print(hex(int.from_bytes(ret[2:6], byteorder='little', signed=False)))
    print("Max cmd size")
    print(int.from_bytes(ret[6:10], byteorder='little', signed=False))


def flash_info(dev):
    ret = send_command(dev, FLASH_INFO, 42)

    print("Flash Information")
    print(ret)
    print("Flash 1 size")
    print(int.from_bytes(ret[2:6], byteorder='little', signed=False))
    print("Flash 2 size")
    print(int.from_bytes(ret[6:10], byteorder='little', signed=False))
    print("Product name")
    print(''.join([chr(x) for x in ret[10:18]]))
    print("GPA/bootloader ver")
    ver = int.from_bytes(ret[18:22], byteorder='little', signed=False)
    print(ver)
    print(hex(ver))
    print("GPA/bootloader start address")
    add = int.from_bytes(ret[22:26], byteorder='little', signed=False)
    print(add)
    print(hex(add))
    print("GPA/bootloader size")
    size = int.from_bytes(ret[26:30], byteorder='little', signed=False)
    print(size)
    print(hex(size))


def otp_info(dev):
    print("OTP read")

    print("HW setting")
    otp_read = [0x23, 0x4, 0x00, 0x01, 0x00]
    ret = send_command(dev, otp_read, 16)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("Force BROM: " + str(ret[2] & 0x01))
    print("Debug disable: " + str(ret[2] & 0x02))
    print("Debug secure: " + str(ret[2] & 0x04))
    print("Extclk disable: " + str(ret[2] & 0x08))

    print("BROM sett1")
    otp_read = [0x23, 0x7, 0x00, 0x01, 0x00]
    ret = send_command(dev, otp_read, 16)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("Secure boot: " + str(ret[2] & 0x01))
    print("Secure boot signature sel: " + str(ret[2] & 0x02))
    print("Secure boot app first: " + str(ret[2] & 0x04))
    print("Secure boot warm reset disable: " + str(ret[2] & 0x08))

    print("BROM sett3")
    otp_read = [0x23, 0x09, 0x00, 0x01, 0x00]
    ret = send_command(dev, otp_read, 16)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print("secure_channel_enable: " + str(ret[2] & 0x01))
    print("secure_algo_SHA256_disable: " + str(ret[2] & 0x02))
    print("secure_algo_HMAC_disable: " + str(ret[2] & 0x04))
    print("secure_spi_slave_enable: " + str(ret[2] & 0x08))
    print("anti_rollback_enable: " + str(ret[2] & 0x10))
    print("download_sram_disable: " + str(ret[2] & 0x20))
    print("otp_sk_d3_no_read: " + str(ret[2] & 0x40))
    print("otp_sk_d2_no_read: " + str(ret[2] & 0x80))
    print("otp_sk_d_no_read: " + str(ret[3] & 0x01))

    print(ret)



    print("SK_D")
    otp_read = [0x23, 0x4c, 0x00, 0x08, 0x00]
    ret = send_command(dev, otp_read, 16*8)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("SK_D2")
    otp_read = [0x23, 0x54, 0x00, 0x08, 0x00]
    ret = send_command(dev, otp_read, 16*8)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("SK_D3")
    otp_read = [0x23, 0x5c, 0x00, 0x08, 0x00]
    ret = send_command(dev, otp_read, 16*8)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("EGIS_SKEY")
    otp_read = [0x23, 0x64, 0x00, 0x08, 0x00]
    ret = send_command(dev, otp_read, 16*8)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("EGIS_SKEY2")
    otp_read = [0x23, 0x6c, 0x00, 0x08, 0x00]
    ret = send_command(dev, otp_read, 16*8)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("EGIS_SKEY3")
    otp_read = [0x23, 0x74, 0x00, 0x08, 0x00]
    ret = send_command(dev, otp_read, 16*8)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

def otp_dump(dev):
    print("OTP dump")

    size = 0
    total_len = 512
    while size < total_len:
        chank = 16
        off = list((size//4).to_bytes(2, byteorder="little"))
        otp_read = [0x23] + off + [0x04, 0x00]
        ret = send_command(dev, otp_read, 16*4)
        size += chank
        if ret[0] != 0:
            print("Incorrect resp format")
            return
        if ret[1] != 0:
            print("Error status: " + str(ret[1]))
            return
        print(ret[2:chank+2])


def flash_read_1m(dev):
    print("Flash read 1mb")

    with open("et171_flash1mb_" + str(int(time.time())) + ".bin",'wb') as f:
#    with open("et171_flash1mb_" + ".bin", 'wb') as f:
        size = 0
        len_chunk = 1024
        total_len = 1 * 1024 * 1024
        done = 0
        while size < total_len:
            off = list((0x80000000 + size).to_bytes(4, byteorder="little"))
            length = list(len_chunk.to_bytes(4, byteorder="little"))
            flash_read = [0x20] + off + [0x01] + length
            ret = send_command(dev, flash_read, 2 + len_chunk)
            size += len_chunk
            if ret[0] != 0:
                print("Incorrect resp format")
                return
            if ret[1] != 0:
                print("Error status: " + str(ret[1]))
                return
            f.write(ret[2:])
            done_tmp = int((size * 100)/total_len)
            if done_tmp != done:
                done = done_tmp
                print("Done " + str(done) + "%")
            time.sleep(0.01)


def flash_write_image(dev, image_path):
    print("Flash write")

#    with open("et171_flash1mb_1741338965.bin", 'rb') as f:
    with open(image_path, 'rb') as f:
        image = f.read()
        size = len(image)
        done = 0
        done_progress = 0
        len_chunk = 1024 * 32

        if size % len_chunk != 0:
            print("Len chunk and erase not aligned")
        while size > done:
            off = list((0x80000000 + done).to_bytes(4, byteorder="little"))
            length = list(len_chunk.to_bytes(4, byteorder="little"))

            # flash_erase = [0x22] + off + length
            # ret = send_command(dev, flash_erase, 2)
            # if ret[0] != 0 or ret[1] != 0:
            #     print("Incorrect resp erase")
            #     print(ret)
            #     return

            time.sleep(0.001)
            write_size = min(size - done, len_chunk)
            length_write = list(write_size.to_bytes(4, byteorder="little"))
            flash_write = [0x21] + off + [0x01] + length_write + list(image[done:done+write_size])
            ret = send_command(dev, flash_write, 2, timeout = 1000)
            if ret[0] != 0 or ret[1] != 0:
                print("Incorrect resp write")
                print(ret)
                return

            done += len_chunk
            done_progress_tmp = int((done * 100)/size)
            if done_progress_tmp != done_progress:
                done_progress = done_progress_tmp
                print("Done " + str(done_progress) + "%")
            time.sleep(0.01)

def validate_commands_file(data):
    done = 0
    size_of_file = len(data)
    while done < size_of_file:
        if (size_of_file - done) < 38:
            print("No enough data for secure channel header")
            return -1
        if data[done] != 0xf4:
            print("Missing secure channel cmd 0xf4")
            return -1
        lenght = int.from_bytes(data[done+2:done+6], byteorder="little",signed=False)
        done += lenght
        # Secure channel command overload
        done += 38

    if done > size_of_file:
        print("Invalid commands file")
        return -1


def run_secure_commands(dev, image_path):
#    with open("et171_flash1mb_1741338965.bin", 'rb') as f:
    with open(image_path, 'rb') as f:
        print("Flashing from commands file")
        image = f.read()
        size_of_file = len(image)
        validate_commands_file(image)
        done = 0
        last_progress = 0

        while done < size_of_file:
            lenght = int.from_bytes(image[done+2:done+6], byteorder="little",signed=False)
            payload = image[done:done+38+lenght]
            ret = send_command(dev, payload, expected_resp_size = 38+2, timeout = 200)
            if ret[0] != 0 or ret[1] != 0:
                print("Secure channel error")
                print(ret)
                return -1

            done += len(payload)
            progress = int((100 * done) / size_of_file)
            if last_progress != progress:
                last_progress = progress
                print(f"Done {progress}%")

            time.sleep(0.01)


def flash_erase(dev):
    flash_erase_cmd = [0x22]
    address = list((0x80000000 + 0x80000).to_bytes(4, byteorder="little"))
    size = list((4096).to_bytes(4, byteorder="little"))
    flash_erase_cmd += address
    flash_erase_cmd += size

    print("Flash erase")
    ret = send_command(dev, flash_erase_cmd, 2)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)


def sign_firmware(dev):
    sign_cmd = [0x25, 0x01]

    print("Sign firmware")
    ret = send_command(dev, sign_cmd, 2)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)


def system_reset(dev, target):
    reset_cmd = [0x12, target]

    print("System reset")
    ret = dev.write(0x02, reset_cmd)

    # No response


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--image",
        help="Image file used for flashing",
        default=ZEPHYR_ET171_FW,
    )

    parser.add_argument(
        "--read",
        help="Read current fw",
        action="store_true"
    )

    parser.add_argument(
        "--bootrom_info",
        help="Get bootrom info",
        action="store_true"
    )

    parser.add_argument(
        "--flash_info",
        help="Get flash info",
        action="store_true"
    )

    parser.add_argument(
        "--otp_info",
        help="Get OTP info",
        action="store_true"
    )

    parser.add_argument(
        "--otp_dump",
        help="Dump OTP",
        action="store_true"
    )

    parser.add_argument(
        "--erase",
        help="Send erase command",
        action="store_true"
    )

    parser.add_argument(
        "--sign",
        help="Send sign command",
        action="store_true"
    )

    parser.add_argument(
        "--disable_wp",
        help="Clear all flash protection",
        action="store_true"
    )

    parser.add_argument(
        "--reboot",
        help="System reset ET171 to application",
        action="store_true"
    )

    parser.add_argument(
        "--disable_watchdog",
        help="Handle external watchdog by PWM",
        action="store_true"
    )

    parser.add_argument(
        "--commands",
        help="Image with pre-prepared secure channel commands",
        default=None,
    )

    args = parser.parse_args()

    dev = usb.core.find(idVendor=EGIS_VID, idProduct=BOOTROM_PID)
    if dev.is_kernel_driver_active(0):
       print("Kernel driver active")
       return 0

    if args.bootrom_info:
        bootroom_info(dev)
    elif args.flash_info:
        flash_info(dev)
    elif args.otp_info:
        otp_info(dev)
    elif args.otp_dump:
        otp_dump(dev)
    elif args.sign:
        sign_firmware(dev)
    elif args.erase:
        flash_erase(dev)
    elif args.read:
        flash_read_1m(dev)
    elif args.disable_wp:
         set_sr1(dev, 0)
    elif args.reboot:
         system_reset(dev, 1)
    elif args.disable_watchdog:
         enable_pwm(dev)
    elif args.commands != None:
         run_secure_commands(dev, args.commands)
    else:
        flash_write_image(dev, args.image)


if __name__ == '__main__':
    sys.exit(main())

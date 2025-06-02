#!/usr/bin/env python3

from sys import byteorder
import usb.core
import sys
import time
import argparse
import os
from pathlib import Path

EC_DIR = Path(os.path.dirname(os.path.realpath(__file__))).parent
ZEPHYR_ET171_FW = os.path.join(EC_DIR, "build/zephyr/et171/output/ec.bin")

EGIS_VID = 0x1c7a
BOOTROM_PID = 0x1002

BOOT_INFO = [0x10]
FLASH_INFO = [0x11]


def bootroom_info(dev):

    ret = dev.write(0x02, BOOT_INFO)
    ret = dev.read(0x81, 100)

    print("Bootrom info")
    print(ret)
    print("ver")
    print(hex(int.from_bytes(ret[2:6], byteorder='little', signed=False)))
    print("Max cmd size")
    print(int.from_bytes(ret[6:10], byteorder='little', signed=False))


def flash_info(dev):
    ret = dev.write(0x02, FLASH_INFO)
    ret = dev.read(0x81, 100)

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
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
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
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
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
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
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


    otp_read = [0x23, 0x01, 0x00, 0x08, 0x00]

    print("SK_D")
    otp_read = [0x23, 0x4c, 0x00, 0x08, 0x00]
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("SK_D2")
    otp_read = [0x23, 0x54, 0x00, 0x08, 0x00]
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("SK_D3")
    otp_read = [0x23, 0x5c, 0x00, 0x08, 0x00]
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("EGIS_SKEY")
    otp_read = [0x23, 0x64, 0x00, 0x08, 0x00]
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("EGIS_SKEY2")
    otp_read = [0x23, 0x6c, 0x00, 0x08, 0x00]
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)

    print("EGIS_SKEY3")
    otp_read = [0x23, 0x74, 0x00, 0x08, 0x00]
    ret = dev.write(0x02, otp_read)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)
    print(ret)


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
            ret = dev.write(0x02, flash_read)
            ret = dev.read(0x81, len_chunk + 2, 100)
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
        len_chunk = 2 * 4096

        if size % len_chunk != 0:
            print("Len chunk and erase not aligned")
        while size > done:
            off = list((0x80000000 + done).to_bytes(4, byteorder="little"))
            length = list(len_chunk.to_bytes(4, byteorder="little"))
            flash_erase = [0x22] + off + length
            write_size = min(size - done, len_chunk)
            length_write = list(write_size.to_bytes(4, byteorder="little"))
            flash_write = [0x21] + off + [0x01] + length_write + list(image[done:done+write_size])
            ret = dev.write(0x02, flash_erase)
            ret = dev.read(0x81, 2, 100)
            if ret[0] != 0 or ret[1] != 0:
                print("Incorrect resp erase")
                print(ret)
                return

            time.sleep(0.001)
            ret = dev.write(0x02, flash_write)
            ret = dev.read(0x81, 2, 100)
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


def flash_erase(dev):
    flash_erase_cmd = [0x22]
    address = list((0x80000000 + 4096).to_bytes(4, byteorder="little"))
    size = list((4096).to_bytes(4, byteorder="little"))
    flash_erase_cmd += address
    flash_erase_cmd += size

    print("Flash erase")
    ret = dev.write(0x02, flash_erase_cmd)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)


def sign_firmware(dev):
    sign_cmd = [0x25, 0x01]

    print("Sign firmware")
    ret = dev.write(0x02, sign_cmd)
    ret = dev.read(0x81, 100)
    if ret[0] != 0 or ret[1] != 0:
        print("Incorrect resp")
        print(ret)


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
        "--erase",
        help="Send erase command",
        action="store_true"
    )

    parser.add_argument(
        "--sign",
        help="Send sign command",
        action="store_true"
    )

    dev = usb.core.find(idVendor=EGIS_VID, idProduct=BOOTROM_PID)
    if dev.is_kernel_driver_active(0):
       print("Kernel driver active")
       return 0

    args = parser.parse_args()

    if args.bootrom_info:
        bootroom_info(dev)
    elif args.flash_info:
        flash_info(dev)
    elif args.otp_info:
        otp_info(dev)
    elif args.sign:
        sign_firmware(dev)
    elif args.erase:
        flash_erase(dev)
    elif args.read:
        flash_read_1m(dev)
    else:
        flash_write_image(dev, args.image)

    print('\n')


if __name__ == '__main__':
    sys.exit(main())

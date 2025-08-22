# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import time

import ectool_commands as commands


def cmd_get_version(args) -> int:
    get_ver = commands.GetVersionCmd()
    ret = get_ver.run()
    if ret != 0:
        return ret

    print("RO: " + get_ver.response.ro_ver.decode("ascii"))
    print("RW: " + get_ver.response.rw_ver.decode("ascii"))
    print("FWID_RO: " + get_ver.response.fwid_ro.decode("ascii"))
    print(
        "Current image: "
        + commands.ImageType.get_image_name(get_ver.response.curr_image)
    )
    print("FWID_RW: " + get_ver.response.fwid_rw.decode("ascii"))


def cmd_fl_info(args) -> int:
    flash_info = commands.FlashInfoCmd(args.num_banks_desc)
    ret = flash_info.run()
    if ret != 0:
        print("Failed to retrieve flash info")
        return ret

    print("Total flash: " + str(flash_info.response.flash_size))
    print("Flags: " + hex(flash_info.response.flags))
    print("Maximum size to write: " + str(flash_info.response.write_ideal_size))
    print(
        "Number of banks present: " + str(flash_info.response.num_banks_total)
    )
    print(
        "Number of banks described: " + str(flash_info.response.num_banks_desc)
    )

    for i in range(flash_info.response.num_banks_desc):
        print(f"Bank {i}:")
        print(
            "\tNumber of sectors: "
            + str(getattr(flash_info.response, f"count{i}"))
        )
        print(
            "\tSize of sector (in power of 2): "
            + hex(getattr(flash_info.response, f"size_exp{i}"))
        )
        print(
            "\tMinimal write size (in power of 2): "
            + hex(getattr(flash_info.response, f"write_size_exp{i}"))
        )
        print(
            "\tErase size (in power of 2): "
            + hex(getattr(flash_info.response, f"erase_size_exp{i}"))
        )
        print(
            "\tSize for write protection (in power of 2): "
            + hex(getattr(flash_info.response, f"protect_size_exp{i}"))
        )
    return ret


def cmd_pr_info(args) -> int:
    pr_info = commands.ProtocolInfoCmd()
    ret = pr_info.run()
    if ret != 0:
        return ret

    print("Version: " + str(pr_info.response.protocol_versions))
    print(
        "Max request packet: " + str(pr_info.response.max_request_packet_size)
    )
    print(
        "Max response packet: " + str(pr_info.response.max_response_packet_size)
    )
    print("Flags: " + hex(pr_info.response.flags))


fp_modes = {
    "deepsleep": 0b1,
    "finger_down": 0b1 << 1,
    "finger_up": 0b1 << 2,
    "mode_capture": 0b1 << 3,
    "enroll_session": 0b1 << 4,
    "enroll_image": 0b1 << 5,
    "match": 0b1 << 6,
    "reset_sensor": 0b1 << 7,
    "sensor_maintenance": 0b1 << 8,
    "dont_change": 0b1 << 31,
}

fp_capture_types = {
    "vendor_format": 0,
    "defect_pxl_test": 1,
    "abnormal_test": 2,
    "noise_test": 3,
    "simple_image": 4,
    "pattern0": 8,
    "pattern1": 12,
    "quality_test": 16,
    "reset_test": 20,
}


def cmd_fp_mode(args) -> int:
    mode = 0
    if args.raw_mode:
        mode = args.raw_mode
    else:
        type = 0
        for arg in args.mode:
            if arg in fp_modes:
                mode |= fp_modes[arg]
            elif arg in fp_capture_types:
                type = fp_capture_types[arg]
        mode |= type << 26
    print("Mode to set: " + hex(mode))
    mode_ec = commands.FpModeCmd(mode)
    ret = mode_ec.run()
    if ret != 0:
        print("Failed to set FP mode")
        return ret
    print("Mode set to: " + hex(mode_ec.response.mode))
    return ret


def cmd_fp_info(args) -> int:
    fp_info = commands.FpInfoCmd()
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


def cmd_fp_vendor(args) -> int:
    vendor_ec = commands.FpVendorCmd(args.param1)
    ret = vendor_ec.run()
    if ret != 0:
        print("Failed to retrieve vendor specyfic data")
    elif not vendor_ec.response or not vendor_ec.response.payload:
        print("Empty vendor specyfic data")
    else:
        print(f"Vedor specyfic data size = {len(vendor_ec.response.payload)}")
        print(vendor_ec.response.payload)
    return ret


def cmd_enter_bootloader(args) -> int:
    enter_bootloader = commands.EnterBootloaderCmd()
    return enter_bootloader.run()


def flash_read_to_file(file: str, offset: int, size: int):
    read_bytes = 0
    ret = 0
    # Get max response size
    pr_info = commands.ProtocolInfoCmd()
    ret = pr_info.run()
    if ret != 0:
        return ret
    max_res_size = (
        pr_info.response.max_response_packet_size - commands.RESPONSE_HEADER_LEN
    )
    with open(file, "wb") as file:
        while read_bytes < size:
            remaining_bytes = size - read_bytes
            if remaining_bytes > max_res_size:
                chunk = max_res_size
            else:
                chunk = remaining_bytes

            flash_read = commands.FlashReadCmd(
                offset=offset + read_bytes, size=chunk
            )
            ret = flash_read.run()
            if ret != 0:
                return ret
            file.write(flash_read.response.data)
            read_bytes += chunk

    return ret


def cmd_flash_read(args) -> int:
    return flash_read_to_file(args.file, args.offset, args.size)


def flash_write_from_file(file: str, offset: int):

    # Get max request size
    pr_info = commands.ProtocolInfoCmd()
    ret = pr_info.run()
    if ret != 0:
        return ret
    max_req_size = (
        pr_info.response.max_request_packet_size
        - commands.REQUEST_HEADER_LEN
        - 8
    )

    with open(file, "rb") as file:
        data = file.read()

    ret = 0
    size = len(data)
    written_bytes = 0

    while written_bytes < size:

        remaining_bytes = size - written_bytes
        if remaining_bytes > max_req_size:
            chunk = max_req_size
        else:
            chunk = remaining_bytes

        flash_write = commands.FlashWriteCmd(
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
    flash_erase = commands.FlashEraseCmd(offset=args.offset, size=args.size)
    return flash_erase.run()


def cmd_flash_region_info(args) -> int:
    flash_region_info = commands.FlashRegionInfoCmd(region=args.region)
    ret = flash_region_info.run()

    if ret != 0:
        return ret

    print("Offset: " + hex(flash_region_info.response.offset))
    print("Size: " + hex(flash_region_info.response.size))


def cmd_reboot_ec(args) -> int:
    reboot_ec = commands.RebootECCmd(cmd=args.cmd)
    return reboot_ec.run()


def cmd_reflash_rw(args) -> int:
    reboot_ec = commands.RebootECCmd(cmd=commands.ECRebootCmd.JUMP_RO)
    ret = reboot_ec.run()
    if ret != 0:
        print("Failed to jump to RO")
        return ret

    time.sleep(2)
    get_ver = commands.GetVersionCmd()
    ret = get_ver.run()
    if ret != 0:
        print("Failed to get version after jump to RO")
        return ret

    if get_ver.response.curr_image != commands.ImageType.RO:
        print("Failed to stay in RO")
        return -1

    print("Stayed in RO after sysjump")

    flash_region_info = commands.FlashRegionInfoCmd(
        region=commands.FlashRegion.UPDATE
    )
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
        return ret

    print(f"RW stored in {rw_file}")

    erase_offset = flash_region_info.response.offset
    while (
        erase_offset
        < flash_region_info.response.offset + flash_region_info.response.size
    ):
        chunk = 1024 * 8
        flash_erase = commands.FlashEraseCmd(offset=erase_offset, size=chunk)
        ret = flash_erase.run()
        # In progress
        if ret == 8:
            time.sleep(0.1)
        elif ret != 0:
            print("Failed to flash erase: " + hex(erase_offset))
            return ret

        erase_offset += chunk

    print("RW erased")
    reboot_ec = commands.RebootECCmd(cmd=commands.ECRebootCmd.COLD)
    ret = reboot_ec.run()
    if ret != 0:
        print("Failed to reboot after erase")
        return ret

    time.sleep(3)
    ret = get_ver.run()
    if ret != 0:
        print("Failed to get version after reboot")
        return ret

    if get_ver.response.curr_image != commands.ImageType.RO:
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
        print("Failed to get version after RW write")
        return ret

    if get_ver.response.curr_image != commands.ImageType.RW:
        print("Failed to jump to RW")
        return -1

    print("Done")


def add_arg_file(sub_parser: argparse.ArgumentParser):
    sub_parser.add_argument("file", type=str)


def auto_int(x):
    return int(x, 0)


def add_arg_offset(sub_parser: argparse.ArgumentParser):
    sub_parser.add_argument("offset", type=auto_int)


def add_arg_size(sub_parser: argparse.ArgumentParser):
    sub_parser.add_argument("size", type=auto_int)


def main():
    parser = argparse.ArgumentParser(
        description="USB version of ectool",
    )

    subcmd = parser.add_subparsers(
        dest="command", help="Host command to run", required=True
    )

    sub_get_version = subcmd.add_parser("version", help="Get version")
    sub_get_version.set_defaults(func=cmd_get_version)

    sub_fl_info = subcmd.add_parser("flinfo", help="Flash info")
    sub_fl_info.add_argument("num_banks_desc", type=auto_int)
    sub_fl_info.set_defaults(func=cmd_fl_info)

    sub_pr_info = subcmd.add_parser("prinfo", help="Protocol info")
    sub_pr_info.set_defaults(func=cmd_pr_info)

    sub_fp_mode = subcmd.add_parser("fpmode", help="FP mode")

    sub_fp_mode.add_argument("--raw_mode", type=auto_int)
    sub_fp_mode.add_argument(
        "mode",
        type=str,
        nargs="*",
        choices=list(fp_modes.keys()) + list(fp_capture_types.keys()),
    )
    sub_fp_mode.set_defaults(func=cmd_fp_mode)

    sub_fp_info = subcmd.add_parser("fpinfo", help="FP info")
    sub_fp_info.set_defaults(func=cmd_fp_info)

    sub_fp_vendor = subcmd.add_parser(
        "fpvendor", help="Vendor specific command"
    )
    sub_fp_vendor.add_argument("param1", type=auto_int)
    sub_fp_vendor.set_defaults(func=cmd_fp_vendor)

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
        type=commands.FlashRegion.from_string,
        choices=list(commands.FlashRegion),
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
        type=commands.ECRebootCmd.from_string,
        choices=list(commands.ECRebootCmd),
        help="Reboot command",
    )
    sub_reboot_ec.set_defaults(func=cmd_reboot_ec)

    sub_reflash_rw = subcmd.add_parser("reflash_rw", help="Try reflashing rw")
    sub_reflash_rw.set_defaults(func=cmd_reflash_rw)

    args = parser.parse_args()

    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

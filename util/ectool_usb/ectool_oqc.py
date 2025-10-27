# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
from enum import IntEnum
import json
import sys

import ectool_commands as commands


class OqcTestError(IntEnum):
    """OQC test error codes."""

    SUCCESS = 0
    SW_INTEGRITY_PROBLEM = 1
    VENDOR_DATA_PROBLEM = 2
    WRONG_CHECKSUM = 3
    CONFIG_CHECK_FAILED = 4


expected_size = 64
data_to_xor = [range(0x0, 0x35 + 1)]
check_sums = [
    (0x0B, [range(0x00, 0x0A + 1)]),
    (0x18, [range(0x0C, 0x17 + 1)]),
    (0x28, [range(0x1E, 0x27 + 1), range(0x29, 0x35 + 1)]),
    (0x36, [range(0x37, 0x3F + 1)]),
]

operations = {"and": lambda x, y: x & y, "equal": lambda x, y: x == y}


def run_oqc(configuration_file) -> int:
    # check version to confirm if RW is running
    get_ver = commands.GetVersionCmd()
    ret = get_ver.run()
    if ret != 0:
        print("Failed to get version")
        return OqcTestError.SW_INTEGRITY_PROBLEM
    if get_ver.response.curr_image != commands.ImageType.RW:
        print("Not in RW")
        return OqcTestError.SW_INTEGRITY_PROBLEM
    print("Integrity OK")

    # get vendor specific data
    vendor_ec = commands.FpVendorCmd(0)
    ret = vendor_ec.run()
    if ret != 0:
        print("Failed to retrieve vendor specyfic data")
        return OqcTestError.VENDOR_DATA_PROBLEM
    elif not vendor_ec.response or not vendor_ec.response.payload:
        print("Empty vendor specyfic data")
        return OqcTestError.VENDOR_DATA_PROBLEM

    # check data size
    if len(vendor_ec.response.payload) == expected_size:
        print(f"Vedor specyfic data size OK")
    else:
        print(
            f"Wrong vedor specyfic data size {len(vendor_ec.response.payload)}"
        )
        return OqcTestError.VENDOR_DATA_PROBLEM

    # apply XOR
    data = list(vendor_ec.response.payload)
    for range in data_to_xor:
        for offset in range:
            data[offset] ^= 0xFF

    # validate checksums
    for check_sum_offset, ranges in check_sums:
        check_sum = 0x00
        for range in ranges:
            for offset in range:
                check_sum += data[offset]
        check_sum &= 0xFF
        if check_sum != data[check_sum_offset]:
            print(
                f"Wrong check sum ({check_sum:#x}) at {check_sum_offset:#x} for {ranges}"
            )
            return OqcTestError.WRONG_CHECKSUM
    print(f"Check sums OK")

    # verify against configuration file
    if configuration_file:
        configuration = json.load(configuration_file)
        for c in configuration:
            offset = int(str(c["offset"]), 0)
            size = int(str(c["size"]), 0)
            val = int.from_bytes(data[offset : offset + size])
            res = val
            for operation in c["operations"]:
                for op, x in operation.items():
                    res = operations[op](res, int(str(x), 0))
            if not res:
                print(f"Check {c} failed for {val:#x}")
                return OqcTestError.CONFIG_CHECK_FAILED
        print(f"Check against configuration file OK")

    print("OQC Pass")
    return OqcTestError.SUCCESS


def cmd_oqc(args) -> int:
    return run_oqc(args.configuration_file)


def main():
    parser = argparse.ArgumentParser(
        description="Run OQC",
    )
    parser.set_defaults(func=cmd_oqc)
    parser.add_argument(
        "configuration_file", nargs="?", type=argparse.FileType("r")
    )
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

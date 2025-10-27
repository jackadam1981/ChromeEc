# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import csv
from enum import IntEnum
from enum import StrEnum
import json
import sys

import ectool_commands as commands
import ectool_communication as communication


class OqcTestError(IntEnum):
    """OQC test error codes."""

    SUCCESS = 0
    SW_INTEGRITY_PROBLEM = 1
    VENDOR_DATA_PROBLEM = 2
    WRONG_CHECKSUM = 3
    CONFIG_CHECK_FAILED = 4
    WRONG_PID = 5


class OqcTestStages(StrEnum):
    SERIAL = "Serial number check"
    VID_PID = "VID and PID check"
    RO_VERSION = "RO version check"
    RW_VERSION = "RW version check"
    SENSOR_DATA = "Sensor data check"


class OqcTestResult(StrEnum):
    PASS = "PASS"
    FAIL = "FAIL"
    NO_TEST = "NO TEST"


expected_size = 64
data_to_xor = [range(0x0, 0x35 + 1)]
check_sums = [
    (0x0B, [range(0x00, 0x0A + 1)]),
    (0x18, [range(0x0C, 0x17 + 1)]),
    (0x28, [range(0x1E, 0x27 + 1), range(0x29, 0x35 + 1)]),
    (0x36, [range(0x37, 0x3F + 1)]),
]

operations = {"and": lambda x, y: x & y, "equal": lambda x, y: x == y}


def _check_serial(comm, results) -> int:
    """Checks serial number."""
    results[OqcTestStages.SERIAL] = (
        f"SN: {comm.serial_number}",
        OqcTestResult.PASS,
    )
    return OqcTestError.SUCCESS


def _check_vid_pid(configuration, comm, results) -> int:
    """Checks VID and PID."""
    if "pid" in configuration:
        expected_pid = int(str(configuration["pid"]), 0)
        if expected_pid != comm.pid:
            results[OqcTestStages.VID_PID] = (
                f"Wrong VID: {comm.pid:X} expected: {expected_pid:X}",
                OqcTestResult.FAIL,
            )
            return OqcTestError.WRONG_PID

    results[OqcTestStages.VID_PID] = (
        f"VID: {comm.vid:X} PID: {comm.pid:X}",
        OqcTestResult.PASS,
    )
    return OqcTestError.SUCCESS


def _check_versions(comm, results) -> int:
    """Checks RO and RW firmware versions."""
    get_ver = commands.GetVersionCmd()
    ret = get_ver.run(comm)
    if ret != 0:
        results[OqcTestStages.RO_VERSION] = (
            "Failed to get version",
            OqcTestResult.FAIL,
        )
        results[OqcTestStages.RW_VERSION] = results[OqcTestStages.RO_VERSION]
        return OqcTestError.SW_INTEGRITY_PROBLEM
    if get_ver.response.curr_image != commands.ImageType.RW:
        results[OqcTestStages.RO_VERSION] = ("Not in RW", OqcTestResult.FAIL)
        results[OqcTestStages.RW_VERSION] = results[OqcTestStages.RO_VERSION]
        return OqcTestError.SW_INTEGRITY_PROBLEM

    ro_version = get_ver.response.ro_ver.decode("ascii").rstrip("\x00")
    results[OqcTestStages.RO_VERSION] = (
        f"RO: {ro_version}",
        OqcTestResult.PASS,
    )
    rw_version = get_ver.response.rw_ver.decode("ascii").rstrip("\x00")
    results[OqcTestStages.RW_VERSION] = (
        f"RW: {rw_version}",
        OqcTestResult.PASS,
    )
    return OqcTestError.SUCCESS


def _calculate_checksum(data, ranges):
    """Helper function to calculate checksum over given data ranges."""
    check_sum = 0x00
    for data_range in ranges:
        for offset in data_range:
            check_sum += data[offset]
    return check_sum & 0xFF


def _check_vendor_data(configuration, comm, results) -> int:
    """Checks vendor specific data."""
    # get vendor specific data
    vendor_ec = commands.FpVendorCmd(0)
    ret = vendor_ec.run(comm)
    if ret != 0:
        results[OqcTestStages.SENSOR_DATA] = (
            "Failed to retrieve vendor specyfic data",
            OqcTestResult.FAIL,
        )
        return OqcTestError.VENDOR_DATA_PROBLEM
    elif not vendor_ec.response or not vendor_ec.response.payload:
        results[OqcTestStages.SENSOR_DATA] = (
            "Empty vendor specyfic data",
            OqcTestResult.FAIL,
        )
        return OqcTestError.VENDOR_DATA_PROBLEM

    # check data size
    if len(vendor_ec.response.payload) != expected_size:
        results[OqcTestStages.SENSOR_DATA] = (
            f"Wrong vedor specyfic data size {len(vendor_ec.response.payload)}",
            OqcTestResult.FAIL,
        )
        return OqcTestError.VENDOR_DATA_PROBLEM

    # apply XOR
    data = list(vendor_ec.response.payload)
    for data_range in data_to_xor:
        for offset in data_range:
            data[offset] ^= 0xFF

    # validate checksums
    for check_sum_offset, ranges in check_sums:
        check_sum = _calculate_checksum(data, ranges)
        if check_sum != data[check_sum_offset]:
            results[OqcTestStages.SENSOR_DATA] = (
                f"Wrong check sum ({check_sum:#x}) at {check_sum_offset:#x} for {ranges}",
                OqcTestResult.FAIL,
            )
            return OqcTestError.WRONG_CHECKSUM

    # verify against configuration file
    if "checks" in configuration:
        for c in configuration["checks"]:
            offset = int(str(c["offset"]), 0)
            size = int(str(c["size"]), 0)
            val = int.from_bytes(data[offset : offset + size])
            res = val
            for operation in c["operations"]:
                for op, x in operation.items():
                    res = operations[op](res, int(str(x), 0))
            if not res:
                results[OqcTestStages.SENSOR_DATA] = (
                    f"Check {c} failed for {val:#x}",
                    OqcTestResult.FAIL,
                )
                return OqcTestError.CONFIG_CHECK_FAILED

    results[OqcTestStages.SENSOR_DATA] = (
        "Check of sensor data OK",
        OqcTestResult.PASS,
    )
    return OqcTestError.SUCCESS


def run_oqc(configuration, comm, results) -> int:
    """Runs all OQC checks."""
    checks = [
        lambda: _check_serial(comm, results),
        lambda: _check_vid_pid(configuration, comm, results),
        lambda: _check_versions(comm, results),
        lambda: _check_vendor_data(configuration, comm, results),
    ]

    for check in checks:
        ret = check()
        if ret != OqcTestError.SUCCESS:
            return ret

    return OqcTestError.SUCCESS


def cmd_oqc(args) -> int:
    ret = 0
    configuration = {}
    if args.configuration_file:
        configuration = json.load(args.configuration_file)
    results = {stage: ("N/A", OqcTestResult.NO_TEST) for stage in OqcTestStages}
    try:
        with communication.UsbCommunication() as comm:
            ret = run_oqc(configuration, comm, results)
    except Exception as e:
        print(f"{e}")
    finally:
        with open(args.output, "w", newline="") as csvfile:
            csv_writer = csv.writer(csvfile)
            csv_writer.writerow(["Stage", "Message", "Result"])
            overall_result = OqcTestResult.PASS
            for stage, (message, result) in results.items():
                print(f"{str(stage)} result {result} with {message}")
                csv_writer.writerow([stage, message, result])
                if result != OqcTestResult.PASS:
                    overall_result = OqcTestResult.FAIL
            csv_writer.writerow(["Overall result", "", overall_result])
    return ret


def main():
    parser = argparse.ArgumentParser(
        description="Run OQC",
    )
    parser.set_defaults(func=cmd_oqc)
    parser.add_argument(
        "configuration_file", nargs="?", type=argparse.FileType("r")
    )
    parser.add_argument(
        "-o", "--output", default="oqc_results.csv", help="Output CSV file name"
    )
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

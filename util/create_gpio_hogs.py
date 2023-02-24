#!/usr/bin/env python3

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create GPIO hog entries matching those setup with a named-gpio node"""

import argparse
import logging

import sys

ZEPHYR_BASE = (
    "/usr/local/google/home/keithshort/chromiumos/src/third_party/zephyr/main/"
)
PLATFORM_EC_BASE = (
    "/usr/local/google/home/keithshort/chromiumos/src/platform/ec/"
)

sys.path.append(ZEPHYR_BASE + "scripts/dts/python-devicetree/src/")

from devicetree import edtlib


gpio_flag_by_mask = dict()
gpio_flag_by_mask[1 << 0] = "GPIO_ACTIVE_LOW"
gpio_flag_by_mask[1 << 1] = "GPIO_SINGLE_ENDED"
gpio_flag_by_mask[1 << 2] = "GPIO_LINE_OPEN_DRAIN"
gpio_flag_by_mask[1 << 4] = "GPIO_PULL_UP"
gpio_flag_by_mask[1 << 5] = "GPIO_PULL_DOWN"
gpio_flag_by_mask[1 << 11] = "GPIO_VOLTAGE_1P8"
gpio_flag_by_mask[1 << 16] = "GPIO_INPUT"
gpio_flag_by_mask[1 << 17] = "GPIO_OUTPUT"
gpio_flag_by_mask[1 << 18] = "GPIO_OUTPUT_INIT_LOW"
gpio_flag_by_mask[1 << 19] = "GPIO_OUTPUT_INIT_HIGH"
gpio_flag_by_mask[1 << 20] = "GPIO_OUTPUT_INIT_LOGICAL"

gpio_mask_by_flag = dict()
for gpio_mask, flag in gpio_flag_by_mask.items():
    gpio_mask_by_flag[flag] = gpio_mask


def _parse_flags(flags):
    # Converts the flags integer value to the equivalent GPIO_ macro
    # and sets the "input", "output-high, and "output-low" properties

    hog_flags = dict()

    # gpio_dt_flags is limited to 16-bits
    dt_flags = ""
    for bit in range(16):
        mask = 1 << bit
        if mask in gpio_flag_by_mask:
            if flags & mask:
                if dt_flags != "":
                    dt_flags += " | "
                dt_flags += gpio_flag_by_mask[mask]

    if dt_flags != "":
        dt_flags = "(" + dt_flags + ")"
    else:
        dt_flags = "0"

    hog_flags["dt_flags"] = dt_flags
    hog_flags["input"] = False
    hog_flags["output-low"] = False
    hog_flags["output-high"] = False

    if flags & gpio_mask_by_flag["GPIO_INPUT"]:
        hog_flags["input"] = True

    if flags & gpio_mask_by_flag["GPIO_OUTPUT"]:
        if flags & gpio_mask_by_flag["GPIO_OUTPUT_INIT_LOW"]:
            hog_flags["output-low"] = True

        if flags & gpio_mask_by_flag["GPIO_OUTPUT_INIT_HIGH"]:
            hog_flags["output-high"] = True

        # Note - GPIO hogs always uses the GPIO_OUTPUT_INIT_LOGICAL flag.
        # If the source flags do not set this, we need to invert the output for
        # active low signals.
        if (
            not (flags & gpio_mask_by_flag["GPIO_OUTPUT_INIT_LOGICAL"])
            and flags & gpio_mask_by_flag["GPIO_ACTIVE_LOW"]
        ):
            hog_flags["output-low"] = not hog_flags["output-low"]
            hog_flags["output-high"] = not hog_flags["output-high"]

    return hog_flags


def main():
    """Read devictree file using Zephyr devicetree library"""
    logging.basicConfig(level=logging.INFO, stream=sys.stderr)
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-i",
        "--input-file",
        help="Input devicetree file",
        required=True,
    )
    parser.add_argument(
        "-o",
        "--output-file",
        help="Output devicetree file containing the GPIO hogs nodes",
        required=True,
    )

    args = parser.parse_args()

    # logging.info("Filename %s", args.input_file)
    bindings_dirs = [
        ZEPHYR_BASE + "include",
        ZEPHYR_BASE + "include/zephyr",
        ZEPHYR_BASE + "dts/common",
        ZEPHYR_BASE + "dts",
        PLATFORM_EC_BASE + "zephyr/dts",
    ]

    try:
        edt = edtlib.EDT(
            args.input_file, bindings_dirs, warn_reg_unit_address_mismatch=False
        )
    except edtlib.EDTError as err:
        sys.exit(f"devicetree error: {err}")

    named_gpios_node = edt.get_node("/named-gpios")

    gpio_ctlrs = dict()
    for node in named_gpios_node.children.values():
        if "gpios" not in node.props:
            logging.warning("gpios property missing from node %s", node.name)
            continue

        gpios = node.props["gpios"].val

        if "enum-name" in node.props:
            enum_name = node.props["enum-name"].val
        else:
            enum_name = None

        for gpio in gpios:
            gpio.data["enum-name"] = enum_name
            gpio.data["nodename"] = node.name

            if gpio.controller.labels[0] is not None:
                gpio_ctrl_nodename = "&" + gpio.controller.labels[0]
            else:
                gpio_ctrl_nodename = gpio.controller.name
            if not gpio_ctrl_nodename in gpio_ctlrs:
                logging.info(
                    "Found new GPIO controller: %s : %s",
                    gpio_ctrl_nodename,
                    gpio.controller.path,
                )
                gpio_ctlrs[gpio_ctrl_nodename] = list()

            gpio_ctlrs[gpio_ctrl_nodename].append(gpio.data)

    with open(args.output_file, "w") as f:
        print("/* GPIO Hogs */", file=f)

        for gpio_ctrl in gpio_ctlrs:
            print(f"{gpio_ctrl} {{", file=f)
            for gpio in gpio_ctlrs[gpio_ctrl]:
                hog_flags = _parse_flags(gpio["flags"])

                print(f"\t{gpio['nodename']} {{", file=f)
                print("\t\tgpio-hog;", file=f)
                print(
                    f"\t\tgpios = <{gpio['pin']} {hog_flags['dt_flags']}>;",
                    file=f,
                )
                if hog_flags["input"]:
                    print("\t\tinput;", file=f)
                if hog_flags["output-low"]:
                    print("\t\toutput-low;", file=f)
                if hog_flags["output-high"]:
                    print("\t\toutput-high;", file=f)
                if gpio["enum-name"] is not None:
                    print(f"\t\tline-names = \"{gpio['enum-name']}\";", file=f)
                print("\t};", file=f)
            print("};", file=f)

        logging.info("GPIO hogs written to %s successfully", args.output_file)


if __name__ == "__main__":
    sys.exit(main())

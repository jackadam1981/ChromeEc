#!/usr/bin/env python3

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uploads an image on to an fpmcu dev board connected via servo micro.

This script assumes you have a servo deamon running with the board that is being tested.

Start servod and JLink locally:

(local chroot) $ sudo servod --board dragonclaw

Upload the image file:

(local chroot) ./util/fpmcu_loadimage.py numpixels /tmp/fpimages/img1.png
"""
import argparse
import sys, getopt
import logging
import subprocess
import socket
import re
import io
import os
from PIL import Image
from dataclasses import dataclass, field
from typing import BinaryIO, Dict, List, Optional, Tuple
from contextlib2 import ExitStack

@dataclass
class BoardConfig:
    """Board-specific configuration."""

    name: str
    servo_uart_name: str
    servo_power_enable: str
    rollback_region0_regex: object
    rollback_region1_regex: object
    mpu_regex: object
    variants: Dict

DATA_ACCESS_VIOLATION_8020000_REGEX = re.compile(
    r"Data access violation, mfar = 8020000\r\n"
)
DATA_ACCESS_VIOLATION_8040000_REGEX = re.compile(
    r"Data access violation, mfar = 8040000\r\n"
)
DATA_ACCESS_VIOLATION_80C0000_REGEX = re.compile(
    r"Data access violation, mfar = 80c0000\r\n"
)
DATA_ACCESS_VIOLATION_80E0000_REGEX = re.compile(
    r"Data access violation, mfar = 80e0000\r\n"
)
DATA_ACCESS_VIOLATION_20000000_REGEX = re.compile(
    r"Data access violation, mfar = 20000000\r\n"
)
DATA_ACCESS_VIOLATION_24000000_REGEX = re.compile(
    r"Data access violation, mfar = 24000000\r\n"
)
PRINTF_CALLED_REGEX = re.compile(r"printf called\r\n")

BLOONCHIPPER = "bloonchipper"
DARTMONKEY = "dartmonkey"

JTRACE = "jtrace"
SERVO_MICRO = "servo_micro"

GCC = "gcc"
CLANG = "clang"

TEST_ASSETS_BUCKET = "gs://chromiumos-test-assets-public/fpmcu/RO"
DARTMONKEY_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "dartmonkey_v2.0.2887-311310808.bin"
)
NOCTURNE_FP_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "nocturne_fp_v2.2.64-58cf5974e.bin"
)
NAMI_FP_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "nami_fp_v2.2.144-7a08e07eb.bin"
)
BLOONCHIPPER_V4277_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "bloonchipper_v2.0.4277-9f652bb3.bin"
)
BLOONCHIPPER_V5938_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "bloonchipper_v2.0.5938-197506c1.bin"
)
BLOONCHIPPER_CONFIG = BoardConfig(
    name=BLOONCHIPPER,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    rollback_region0_regex=DATA_ACCESS_VIOLATION_8020000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_8040000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_20000000_REGEX,
    variants={
        "bloonchipper_v2.0.4277": {
            "ro_image_path": BLOONCHIPPER_V4277_IMAGE_PATH
        },
        "bloonchipper_v2.0.5938": {
            "ro_image_path": BLOONCHIPPER_V5938_IMAGE_PATH
        },
    },
)

DARTMONKEY_CONFIG = BoardConfig(
    name=DARTMONKEY,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    rollback_region0_regex=DATA_ACCESS_VIOLATION_80C0000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_80E0000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_24000000_REGEX,
    # For dartmonkey board, run panic data test also on nocturne_fp and
    # nami_fp boards with appropriate RO image.
    variants={
        "dartmonkey_v2.0.2887": {"ro_image_path": DARTMONKEY_IMAGE_PATH},
        "nocturne_fp_v2.2.64": {
            "ro_image_path": NOCTURNE_FP_IMAGE_PATH,
            "build_board": "nocturne_fp",
        },
        "nami_fp_v2.2.144": {
            "ro_image_path": NAMI_FP_IMAGE_PATH,
            "build_board": "nami_fp",
        },
    },
)

BOARD_CONFIGS = {
    "bloonchipper": BLOONCHIPPER_CONFIG,
    "dartmonkey": DARTMONKEY_CONFIG,
}

def parse_remote_arg(remote: str) -> str:
    """Convert the 'remote' input argument to IP address, if available."""
    if not remote:
        return ""

    try:
        ip_addr = socket.gethostbyname(remote)
        return ip_addr
    except socket.gaierror:
        logging.error('Failed to resolve host "%s".', remote)
        sys.exit(1)

def get_console(board_config: BoardConfig) -> Optional[str]:
    """Get the name of the console for a given board."""
    cmd = [
        "dut-control",
        board_config.servo_uart_name,
    ]
    logging.debug('Running command: "%s"', " ".join(cmd))

    with subprocess.Popen(cmd, stdout=subprocess.PIPE) as proc:
        for line in io.TextIOWrapper(proc.stdout):  # type: ignore[arg-type]
            logging.debug(line)
            pty = line.split(":")
            if len(pty) == 2 and pty[0] == board_config.servo_uart_name:
                return pty[1].strip()

    return None

def main():

    parser = argparse.ArgumentParser()

    default_board = "bloonchipper"
    parser.add_argument(
        "--board",
        "-b",
        help="Board (default: " + default_board + ")",
        default=default_board,
    )
    parser.add_argument(
        "--image",
        "-i",
        help="Image file as PNG",
        default="",
    )
    parser.add_argument(
        "--remote",
        "-n",
        help="The remote host connected to one or both of: J-Link and Servo.",
        type=parse_remote_arg,
    )
    parser.add_argument(
        "--jlink_port",
        "-j",
        type=int,
        help="The port to use when connecting to JLink.",
    )
    parser.add_argument(
        "--console_port",
        "-p",
        type=int,
        help="The port connected to the FPMCU console.",
    )

    args = parser.parse_args()
    board_config = BOARD_CONFIGS[args.board]

    inputimgfile = args.image

    if not inputimgfile.endswith('.png'):
        logging.error("Input file must be png, sorry. you input", inputimgfile)
    img = Image.open(inputimgfile)
    
    #check that image is correct size for fpmcu.
    w, h = img.size
    if BoardConfig  == "bloonchipper":
        if w != h != 160:
            logging.error("This img size is not 160x160")
    elif BoardConfig == "dartmonkey":
        if w != 56 or h != 192:
            logging.error("This img size is not 56x192")
    logging.info("w, h", w,h)

    #open fpmcu console.
    with ExitStack() as stack:
        if args.remote and args.console_port:
            console_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            console_socket.connect((args.remote, args.console_port))
            console = stack.enter_context(
                console_socket.makefile(mode="rwb", buffering=0)
            )
        else:
            console = stack.enter_context(
                open(get_console(board_config), "wb+", buffering=0)
            )

        imgstr = ""
        for i in range(w):
            for j in range(h):
                coordinates = x, y = i, j
                tmp = str(hex(img.getpixel(coordinates)))
                if len(tmp) == 4:
                    imgstr += tmp[-2:]
                else:
                    imgstr += "0" + tmp[2]

        upload_cmd = "fpimageload " + str(int(len(imgstr)/2)) + " " + imgstr + "\n"
        console.write(upload_cmd.encode())


if __name__ == "__main__":
    sys.exit(main())
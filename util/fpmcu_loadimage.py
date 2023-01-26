#!/usr/bin/env python3

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uploads an image on to an FPMCU dev board.

# Requirements

Install the Python dependencies in your chroot:

  (chroot) emerge pillow

Build the EC image with debug mode enabled by adding this to include/config.h:

  #define CONFIG_CMD_FPSENSOR_DEBUG


# Usage

Start servod with the board that is being tested:

   (chroot) $ sudo servod --board dragonclaw

Upload the image file:

   (chroot) ./util/fpmcu_loadimage.py -i /tmp/fpimages/img1.png -b bloonchipper
"""
import argparse
from dataclasses import dataclass
from enum import Enum
import io
import logging
import subprocess
import sys
from typing import Optional
import unittest

from contextlib2 import ExitStack
from PIL import Image


class SensorByteOrder(Enum):
    """Storage order of bytes returned by the sensor"""

    ROW_MAJOR = 1
    COLUMN_MAJOR = 2


# TODO(bobbycasey): Refactor this section of the code to remove duplication
# with run_device_tests.py
@dataclass
class BoardConfig:
    """Board-specific configuration."""

    name: str
    servo_uart_name: str
    sensor_height: int
    sensor_width: int
    sensor_byte_order: SensorByteOrder


BLOONCHIPPER_CONFIG = BoardConfig(
    name="bloonchipper",
    servo_uart_name="raw_fpmcu_console_uart_pty",
    sensor_width=160,
    sensor_height=160,
    sensor_byte_order=SensorByteOrder.ROW_MAJOR,
)

DARTMONKEY_CONFIG = BoardConfig(
    name="dartmonkey",
    servo_uart_name="raw_fpmcu_console_uart_pty",
    sensor_width=56,
    sensor_height=192,
    sensor_byte_order=SensorByteOrder.COLUMN_MAJOR,
)

BOARD_CONFIGS = {
    "bloonchipper": BLOONCHIPPER_CONFIG,
    "dartmonkey": DARTMONKEY_CONFIG,
}


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


def load_image_from_png(image_path: str) -> Image:
    """Load a PNG image from file"""
    if not image_path.endswith(".png"):
        raise ValueError(
            f"Input file must be png, sorry. Input was {image_path}"
        )

    return Image.open(image_path)


def pixel_value_to_hex(value: int) -> str:
    """Convert an 8-bit pixel value to an hex string of two characters"""
    if value < 0 or value > 255:
        raise ValueError(f"Pixel value {value} outside range 0-255")
    return "{:02x}".format(value)


def image_to_hex_str(image: Image, byte_order: SensorByteOrder) -> str:
    """Convert an 8-bit image to a string of hex characters"""
    image_str = ""
    num_cols, num_rows = image.size
    if byte_order == SensorByteOrder.ROW_MAJOR:
        for row in range(num_rows):
            for col in range(num_cols):
                image_str += pixel_value_to_hex(image.getpixel((col, row)))
    elif byte_order == SensorByteOrder.COLUMN_MAJOR:
        for col in range(num_cols):
            for row in range(num_rows):
                image_str += pixel_value_to_hex(image.getpixel((col, row)))

    return image_str


def get_fploadimage_cmd(
    board_config: BoardConfig, image: Image, echo: bool
) -> str:
    """Return the command to upload the image on the FPMCU"""

    # Check that image has the correct size for the board selected.
    expected_size = (board_config.sensor_width, board_config.sensor_height)
    if image.size != expected_size:
        raise ValueError(
            f"Image size is {image.size} but expected {expected_size}"
        )

    logging.info("Image size: %s", image.size)

    # fpimageload takes image as a single string of hex values.
    image_str = image_to_hex_str(image, board_config.sensor_byte_order)

    echo_flag = 1 if echo else 0
    num_pixels = expected_size[0] * expected_size[1]
    return f"fpimageload {echo_flag} {num_pixels} {image_str} \n"


def main():
    """Load a PNG image from file and transfer it to the FPMCU"""

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
        help="Image file in PNG",
        default="",
    )
    parser.add_argument(
        "--echo",
        type=bool,
        help="Debug: whether or not to echo back the input image.",
        default=0,
    )

    args = parser.parse_args()
    board_config = BOARD_CONFIGS[args.board]

    image = load_image_from_png(args.image)
    fploadload_cmd = get_fploadimage_cmd(board_config, image, args.echo)

    with ExitStack() as stack:
        console = stack.enter_context(
            open(get_console(board_config), "wb+", buffering=0)
        )
        console.write(fploadload_cmd.encode())


if __name__ == "__main__":
    sys.exit(main())


# Disable docstrings for unit tests
# pylint: disable=missing-module-docstring
# pylint: disable=missing-class-docstring
# pylint: disable=missing-function-docstring
class TestLoadImageFromPng(unittest.TestCase):
    def test_invalid_file_extension(self):
        with self.assertRaises(ValueError):
            load_image_from_png("not_a_png.jpg")

    def test_empty_file_path(self):
        with self.assertRaises(ValueError):
            load_image_from_png("")


class TestPixelValueToHex(unittest.TestCase):
    def test_value_below_zero(self):
        with self.assertRaises(ValueError):
            pixel_value_to_hex(-1)

    def test_value_above_255(self):
        with self.assertRaises(ValueError):
            pixel_value_to_hex(256)

    def test_valid_values(self):
        self.assertEqual(pixel_value_to_hex(0), "00")
        self.assertEqual(pixel_value_to_hex(1), "01")
        self.assertEqual(pixel_value_to_hex(10), "0a")
        self.assertEqual(pixel_value_to_hex(255), "ff")


class TestImageToHexStr(unittest.TestCase):
    # Use a small image with width = 5 and height = 3
    def test_row_major_order(self):
        image = Image.new("P", (5, 3))
        image.putpixel((0, 1), 255)
        image_str = image_to_hex_str(image, SensorByteOrder.ROW_MAJOR)
        self.assertEqual(len(image_str), 30)
        self.assertEqual(image_str[:2], "00")
        self.assertEqual(image_str[10:12], "ff")

    def test_col_major_order(self):
        image = Image.new("P", (5, 3))
        image.putpixel((0, 1), 255)
        image_str = image_to_hex_str(image, SensorByteOrder.COLUMN_MAJOR)
        self.assertEqual(len(image_str), 30)
        self.assertEqual(image_str[:2], "00")
        self.assertEqual(image_str[2:4], "ff")


class TestGetFpimageloadCmd(unittest.TestCase):
    def test_invalid_size(self):
        image = Image.new("P", (10, 20))
        with self.assertRaises(ValueError):
            get_fploadimage_cmd(DARTMONKEY_CONFIG, image, False)

    def test_valid_size(self):
        image = Image.new("P", (56, 192))
        cmd = get_fploadimage_cmd(DARTMONKEY_CONFIG, image, False)
        self.assertEqual(cmd[:19], "fpimageload 0 10752")

    def test_echo(self):
        image = Image.new("P", (56, 192))
        cmd = get_fploadimage_cmd(DARTMONKEY_CONFIG, image, True)
        self.assertEqual(cmd[:19], "fpimageload 1 10752")

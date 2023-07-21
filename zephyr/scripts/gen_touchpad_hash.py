# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Touchpad firmware hash generator script."""

import argparse
import hashlib
import os.path
import pathlib
import sys
from typing import BinaryIO, TextIO


def print_hash(out: TextIO, digest: [int], end: str = ",") -> None:
    """Convert the digest blob to a C array literal."""

    print("{ ", file=out, end="")
    for val in digest:
        print(f"{val:#04x}, ", file=out, end="")
    print(f"}}{end}", file=out)


def gen_header(
    fw_file: BinaryIO,
    out: TextIO,
    touchpad_virtual_size: int,
    update_pdu_size: int,
):
    """Generate the touchpad hash header using given parameters."""

    digest_size = hashlib.sha256().digest_size
    num_blocks = touchpad_virtual_size // update_pdu_size

    print("#include <stdint.h>\n", file=out)

    print(
        f"const uint8_t touchpad_fw_hashes[{num_blocks}][{digest_size}] = {{",
        file=out,
    )

    for _ in range(num_blocks):
        if fw_file is not None:
            block = fw_file.read(update_pdu_size)
            print_hash(out, hashlib.sha256(block).digest())
        else:
            print_hash(out, [0] * digest_size)

    print("};", file=out)
    print(
        f"const uint8_t touchpad_fw_full_hash[{digest_size}] =\n\t",
        file=out,
        end="",
    )
    if fw_file is not None:
        fw_file.seek(0, 0)
        print_hash(out, hashlib.sha256(fw_file.read()).digest(), ";")
    else:
        print_hash(out, [0] * digest_size, ";")


def main() -> int:
    """The main function."""

    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-t",
        "--touchpad_virtual_size",
        type=int,
        required=True,
        help="Expected size of the touchpad firmware",
    )
    parser.add_argument(
        "-u",
        "--update_pdu_size",
        type=int,
        required=True,
        help="PDU size for firmware update",
    )
    parser.add_argument(
        "-f",
        type=pathlib.Path,
        dest="fw_path",
        help="Firmware file. Will output blank hashes if not provided.",
    )
    parser.add_argument(
        "-o",
        type=pathlib.Path,
        required=True,
        dest="output_path",
        help="Output file",
    )

    args = parser.parse_args()

    args.output_path.parent.mkdir(parents=True, exist_ok=True)

    with args.output_path.open("w") as output:
        if args.fw_path:
            fw_size = os.path.getsize(args.fw_path)
            expected_size = args.touchpad_virtual_size
            if fw_size != expected_size:
                print(
                    f"Incorrect TP FW size ({fw_size} vs {expected_size})",
                    file=sys.stderr,
                )
                return 1
            with args.fw_path.open("rb") as fw_file:
                gen_header(
                    fw_file,
                    output,
                    args.touchpad_virtual_size,
                    args.update_pdu_size,
                )
        else:
            gen_header(
                None, output, args.touchpad_virtual_size, args.update_pdu_size
            )

    return 0


if __name__ == "__main__":
    sys.exit(main())

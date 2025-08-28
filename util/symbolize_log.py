#!/usr/bin/env python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Symbolizes any addresses found in an EC log."""

import argparse
import os
import re
import subprocess
import sys


# Regex to find PC and LR registers and their hex values
# Looks for e.g., PC: 0x1234, lr = 0xabcd, (PC 0x1234), [lr=0xabcd]
# Captures "PC", "LR", "RA", or "SP" (case-insensitive) and the address.
LOG_ADDR_REGEX = re.compile(
    r"""
    (?P<reg>PC|LR|RA|SP)  # Register name PC or LR
    \s*[:=]\s*     # Separator : or =
    (?P<addr>0x[0-9a-fA-F]+) # The hexadecimal address
    """,
    re.IGNORECASE | re.VERBOSE,
)

ADDR2LINE_CACHE = {}


def symbolize_address(addr, elf_file, addr2line_tool):
    """
    Runs addr2line to get symbol information for a given address.
    Caches results.
    """
    if addr in ADDR2LINE_CACHE:
        return ADDR2LINE_CACHE[addr]

    if not os.path.exists(elf_file):
        return f"ELF file not found: {elf_file}"

    try:
        cmd = [
            addr2line_tool,
            "-e",
            elf_file,
            "-a",
            addr,
            "-f",  # Show function names
            "-C",  # Demangle function names
            "-i",  # Show inlines
            "-p",  # Pretty print
        ]
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=True, encoding="utf-8"
        )
        # Output is typically address followed by function/file:line on next line
        output_lines = result.stdout.strip().split("\n")

        if len(output_lines) >= 2:
            # Combine the line after the address
            symbolized = output_lines[1].strip()
            ADDR2LINE_CACHE[addr] = symbolized
            return symbolized

        ADDR2LINE_CACHE[addr] = output_lines
        return output_lines

    except subprocess.CalledProcessError as e:
        error_msg = f"addr2line failed for {addr}: {e.stderr.strip()}"
        ADDR2LINE_CACHE[addr] = error_msg
        return error_msg
    except FileNotFoundError:
        error_msg = f"addr2line tool not found: {addr2line_tool}"
        # No point caching this per address
        raise


def parse_log_line(line, elf_file, addr2line_tool):
    """
    Finds all PC/LR addresses in a line and symbolizes them.
    """
    matches = LOG_ADDR_REGEX.finditer(line)
    symbolized_info = []

    found_addresses = False
    for match in matches:
        found_addresses = True
        reg = match.group("reg").upper()
        addr = match.group("addr")
        try:
            symbol = symbolize_address(addr, elf_file, addr2line_tool)
            symbolized_info.append(f"  {reg} ({addr}): {symbol}")
        except FileNotFoundError as e:
            # Tool not found, print error and exit early.
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

    if found_addresses:
        print(f"[Log Line]: {line.strip()}")
        for info in symbolized_info:
            print(info)
        print("-" * 20)


def main():
    """
    Main entry point
    """
    parser = argparse.ArgumentParser(
        description="Parse log files and run addr2line on PC and LR values."
    )
    parser.add_argument(
        "--elf", required=True, help="Path to the ELF file with debug symbols."
    )
    parser.add_argument(
        "--addr2line",
        default="addr2line",
        help="Path to the addr2line executable.",
    )
    parser.add_argument(
        "log_file",
        nargs="?",
        type=argparse.FileType("r"),
        default=sys.stdin,
        help="Log file to parse (reads from stdin if not specified).",
    )

    args = parser.parse_args()

    if not os.path.exists(args.elf):
        print(f"Error: ELF file not found: {args.elf}", file=sys.stderr)
        sys.exit(1)

    try:
        for line in args.log_file:
            parse_log_line(line, args.elf, args.addr2line)
    except KeyboardInterrupt:
        print("\nExiting...", file=sys.stderr)
    finally:
        if args.log_file != sys.stdin:
            args.log_file.close()


if __name__ == "__main__":
    main()

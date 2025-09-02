#!/usr/bin/env python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""

Find unused source files by comparing elf files to source files.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import fnmatch
import os
from pathlib import Path
import re
import subprocess
import sys


# Find files using python walk
def find_files_python(paths, patterns):
    sources = []
    for path in paths:
        for root, _, files in os.walk(path):
            root = Path(root)
            for pattern in patterns:
                for match in fnmatch.filter(files, pattern):
                    sources.append(root / match)
    return sources


# Find files using native find
def find_files(paths, patterns):
    sources = []
    filter_pattern = []
    for pattern in patterns:
        # Add or, excpet on first pass
        if filter_pattern:
            filter_pattern.append("-o")
        filter_pattern.extend(["-name", pattern])
    for path in paths:
        cmd = ["find", str(path), "-type", "f"] + filter_pattern
        # print(f'* Running command: "{" ".join(cmd)}"')
        result = subprocess.run(
            ["find", path, "-type", "f"] + filter_pattern,
            capture_output=True,
            text=True,
            check=True,
        )
        found_files = [Path(p) for p in result.stdout.splitlines()]
        sources.extend(found_files)
    return sources


def gdb(cmd, elf):
    try:
        result = subprocess.run(
            ["gdb", "-q", "--batch", "--readnow", "--ex", cmd, elf],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error running GDB command: {e}")
        return None


def sources_from_elf(elf) -> list:
    output = gdb(cmd="info sources", elf=elf)
    if not output:
        return []
    sources = [
        Path(source) for source in output.split(":")[1].strip().split(", ")
    ]
    # Filter files like <artificial>
    sources = filter(
        lambda x: not (x.name.startswith("<") and x.name.endswith(">")), sources
    )
    return sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    ec_root = Path("/mnt/host/source/src/platform/ec")
    source_paths = [
        ec_root / "baseboard",
        ec_root / "board",
        ec_root / "builtin",
        ec_root / "chip",
        ec_root / "common",
        ec_root / "core",
        ec_root / "crypto",
        ec_root / "driver",
        ec_root / "extra",
        ec_root / "include",
        ec_root / "libc",
        ec_root / "power",
        ec_root / "test",
        ec_root / "zephyr",
    ]
    build_paths = [
        ec_root / "build",
        ec_root / "twister-out",
    ]
    print("Finding all source files...")
    all_sources = find_files(
        paths=source_paths, patterns=["*.c", "*.h", "*.cc", "*.S", "*.inc"]
    )
    print(f"* Found {len(all_sources)} source files")

    print("Finding elf files...")
    all_elfs = find_files(paths=build_paths, patterns=["*.elf"])
    print(f"* Found {len(all_elfs)} elf files")

    print("Extracting sources from elf files...")
    used_sources = set()
    count = 0
    with ThreadPoolExecutor(max_workers=16) as executor:
        futures = [executor.submit(sources_from_elf, elf) for elf in all_elfs]
        for future in futures:
            count += 1
            used_sources.update(future.result())
            print(f"{count}/{len(all_elfs)}", end="\r")

    print(f"* Extracted {len(used_sources)} source files.")

    unused_sources = []
    unknown_sources = []

    for source in all_sources:
        if source not in used_sources:
            # Can't reliably detect unused header files using this method
            if source.suffix in [".h"]:
                continue
            unused_sources.append(source)

    for source in used_sources:
        if source.parent == ec_root and source not in all_sources:
            unknown_sources.append(source)

    print(f"\nUNUSED SOURCES({len(unused_sources)}):\n")
    for source in unused_sources:
        print(source.relative_to(ec_root))

    print("\nUNKNOWN SOURCES:\n")
    for source in unknown_sources:
        print(source)


if __name__ == "__main__":
    main()

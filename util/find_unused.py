#!/usr/bin/env python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""

Find unused sources, functions and configs by comparing sources to build artficats.

This script relies huristics. Results must be interpreted by a developer.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import fnmatch
import os
from pathlib import Path
import re
import subprocess
import sys


MAX_WORKERS = 64


# Find files using native find
def find_files(paths, patterns):
    sources = set()
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
            cmd,
            capture_output=True,
            text=True,
            check=True,
        )
        found_files = [Path(p) for p in result.stdout.splitlines()]
        sources.update(found_files)
    return sources


# Execute batch gdb command
def gdb(cmd, elf):
    result = subprocess.run(
        ["gdb", "-q", "--batch", "--readnow", "--ex", cmd, elf],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout


# Find sources used in elf
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


# Find functions used in elf
def find_functions_in_elf(elf) -> list:
    # Returns a list of tuples (file, function_name, function_signature)
    output = gdb(cmd="info functions -q -n", elf=elf)
    if not output:
        return []

    file = None
    functions = []

    for line in output.splitlines():
        line = line.strip()
        if not line:
            continue

        # Check if the line declares a new file context
        if line.startswith("File "):
            file = line.removeprefix("File ").removesuffix(":")
            continue
        match = re.match(
            r"(?:[0-9]+:\s+)?([\w:*\s]+\s\**?([\w.:<>~\s,[\]()]+))\(.*\)(?:\s\[[\w\s.]+\])?;",
            line,
        )
        if not match:
            continue
        functions.append((Path(file), match.group(2)))
    return functions


# Finds all configs referenced in the source code
def find_configs(paths) -> set:
    configs = set()
    config_pattern = "CONFIG_[A-Z0-9_]*[A-Z0-9]"
    for path in paths:
        cmd = [
            "grep",
            "--recursive",
            "--no-filename",
            "--binary-files=without-match",
            "--only-matching",
            "--extended-regexp",
            config_pattern,
            str(path),
        ]
        print(f'Running command {" ".join(cmd)}')
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False,
        )
        configs.update(result.stdout.splitlines())
    return configs


# Find all enabled configs in .config build artificats
def find_set_configs(paths) -> set:
    configs = set()
    config_set_pattern = "^CONFIG_[A-Z0-9_]*[A-Z0-9]+=.*"
    for path in paths:
        cmd = [
            "grep",
            "--recursive",
            "--no-filename",
            "--binary-files=without-match",
            "--extended-regexp",
            "--include=ec.config",
            "--include=.config",
            config_set_pattern,
            str(path),
        ]
        print(f'Running command {" ".join(cmd)}')
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=True,
        )
        configs.update([c.split("=")[0] for c in result.stdout.splitlines()])
    return configs


def find_functions_in_file(path):
    # Finda all defined functions in a file
    functions = []
    with open(path, "r") as f:
        content = f.read()
    # functions = re.findall(r'^(?:[\w*]+\s+)+?(\w+)\s*\([\w\s*,]*\)\s*[;{]',content,re.MULTILINE)
    matches = re.findall(
        r"^(\w+[\w:*\s]+\s\**([\w.:]+)\([\w\s,*]*\))\s*{", content, re.MULTILINE
    )
    for match in matches:
        functions.append((path, match[1]))
    # print(functions)
    return functions


def find_functions(paths) -> set:
    functions = set()
    sources = find_files(
        paths=paths, patterns=["*.c", "*.h", "*.cc", "*.S", "*.inc"]
    )
    for source in sources:
        functions.update(find_functions_in_file(source))
    return functions


def normalize_path(ec_root, zephyr_root, path):
    if path.is_relative_to(ec_root):
        path = path.relative_to(ec_root)
    elif path.is_relative_to(zephyr_root):
        path = path.is_relative_to(zephyr_root)
    path = re.sub(r"build/zephyr/[\w-]+/modules/ec/", "", str(path))
    path = re.sub(r"build/zephyr/[\w-]+/build-r[ow]/", "", str(path))
    return Path(path)


def find_unused_functions(ec_root, zephyr_root, source_paths, build_paths):
    print("Finding all functions...")
    all_functions = find_functions(paths=source_paths)
    # Normalize path
    all_functions = set(
        [(f[0].relative_to(ec_root), f[1]) for f in all_functions]
    )
    print(f"* Found {len(all_functions)} functions")

    print(f"\nALL FUNCTIONS({len(all_functions)}):\n")
    for path, function_name in sorted(all_functions):
        print(f"{str(path)}: {function_name}")

    print("Finding elf files...")
    all_elfs = list(find_files(paths=build_paths, patterns=["*.elf"]))
    print(f"* Found {len(all_elfs)} elf files")

    print("Extracting functions in elf files...")
    used_functions = set()
    count = 0
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = [
            executor.submit(find_functions_in_elf, elf) for elf in all_elfs
        ]
        for future in futures:
            count += 1
            used_functions.update(future.result())
            print(f"{count}/{len(all_elfs)}", end="\r")
    print(f"* Extracted {len(used_functions)} functions from elfs")
    # Normalize paths
    used_functions = set(
        [
            (normalize_path(ec_root, zephyr_root, f[0]), f[1])
            for f in used_functions
        ]
    )

    print(f"\nUSED FUNCTIONS({len(used_functions)}):\n")
    for path, function_name in sorted(used_functions):
        print(f"+ {str(path)}: {function_name}")

    unused_functions = all_functions - used_functions
    print(f"\nUNUSED FUNCTIONS({len(unused_functions)}):\n")
    for path, function_name in sorted(unused_functions):
        print(f"- {str(path)}: {function_name}")

    unknown_functions = used_functions - all_functions

    print(f"\nUNKNOWN FUNCTIONS({len(unknown_functions)}):\n")
    # for path, function_name in sorted(unknown_functions):
    #     print(f'! {str(path)}: {function_name}')


def find_unused_configs(source_paths, build_paths):
    print("Finding all referenced configs...")
    all_configs = find_configs(source_paths)
    print(f"* Found {len(all_configs)} configs")

    print("Finding all set configs...")
    set_configs = find_set_configs(build_paths)
    print(f"* Found {len(set_configs)} set configs")

    never_set_configs = all_configs - set_configs
    print(f"\nNEVER SET CONFIGS({len(never_set_configs)}):\n")
    for config in sorted(list(never_set_configs)):
        print(config)

    unknown_configs = set_configs - all_configs
    print(f"\nUNKNOWN CONFIGS({len(unknown_configs)}):\n")
    # for config in sorted(list(unknown_configs)):
    #     print(config)


def find_unused_sources(ec_root, source_paths, build_paths):
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
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = [executor.submit(sources_from_elf, elf) for elf in all_elfs]
        for future in futures:
            count += 1
            used_sources.update(future.result())
            print(f"{count}/{len(all_elfs)}", end="\r")

    print(f"* Extracted {len(used_sources)} source files.")

    unused_sources = []
    unknown_sources = []

    for source in all_sources:
        # Can't reliably detect unused header files using this method
        if source.suffix in [".h"]:
            continue
        if source not in used_sources:
            unused_sources.append(source)

    for source in used_sources:
        if source.parent in source_paths and source not in all_sources:
            unknown_sources.append(source)

    print(f"\nUNUSED SOURCES({len(unused_sources)}):\n")
    for source in unused_sources:
        print(source.relative_to(ec_root))

    print("\nUNKNOWN SOURCES:\n")
    for source in unknown_sources:
        print(source)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["sources", "configs", "functions"])
    args = parser.parse_args()
    ec_root = Path("/mnt/host/source/src/platform/ec")
    zephyr_root = Path("/mnt/host/source/src/third_party/zephyr/main/")
    ec_zephyr_root = ec_root / "zephyr"
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
        ec_zephyr_root / "app",
        ec_zephyr_root / "boards",
        ec_zephyr_root / "chip",
        ec_zephyr_root / "drivers",
        ec_zephyr_root / "dts",
        ec_zephyr_root / "include",
        ec_zephyr_root / "lib",
        ec_zephyr_root / "libc",
        ec_zephyr_root / "program",
        ec_zephyr_root / "shim",
        ec_zephyr_root / "subsys",
    ]
    test_source_paths = [
        ec_root / "test",
        ec_zephyr_root / "test",
        ec_zephyr_root / "fake",
        ec_zephyr_root / "emul",
    ]
    build_paths = [
        ec_root / "build",
    ]
    test_build_paths = [
        ec_root / "twister-out",
    ]

    match args.command:
        case "sources":
            find_unused_sources(
                ec_root=ec_root,
                source_paths=source_paths,
                build_paths=build_paths,
            )
        case "configs":
            find_unused_configs(
                source_paths=source_paths + test_source_paths,
                build_paths=build_paths + test_build_paths,
            )
        case "functions":
            find_unused_functions(
                ec_root=ec_root,
                zephyr_root=zephyr_root,
                source_paths=source_paths,
                build_paths=build_paths,
            )


if __name__ == "__main__":
    main()

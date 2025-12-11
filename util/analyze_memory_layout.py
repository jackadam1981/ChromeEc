#!/usr/bin/env vpython3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Analyze Zephyr RAM and Flash usage details."""

# [VPYTHON:BEGIN]
# wheel: <
#   name: "infra/python/wheels/pyelftools-py2_py3"
#   version: "version:0.29"
# >
# [VPYTHON:END]

import argparse
import json
from pathlib import Path
import sys
from typing import Any, Dict, List, NamedTuple, Optional

# pylint: disable=import-error
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection


class Section(NamedTuple):
    """Represents a memory section."""

    name: str
    start: int
    end: int
    is_unused: bool


class Symbol(NamedTuple):
    """Represents a symbol in the ELF file."""

    name: str
    start: int
    size: int


class BuildInfo:
    """Extract relevant symbols from a Zephyr ELF file."""

    def __init__(self, elf_file_path: Path):
        self.syms = {}
        self.all_symbols: List[Symbol] = []
        self.path = elf_file_path

        with open(elf_file_path, "rb") as file:
            elf = ELFFile(file)
            for section in elf.iter_sections():
                if isinstance(section, SymbolTableSection):
                    for s in section.iter_symbols():
                        self.syms[s.name] = s.entry.st_value
                        if s.entry.st_size > 0:
                            self.all_symbols.append(
                                Symbol(
                                    s.name, s.entry.st_value, s.entry.st_size
                                )
                            )
                    break

    def get_symbol(self, sym, default=None):
        """Get a symbol from the ELF or default if it doesn't exist."""
        return self.syms.get(sym, default)

    def get_large_symbols(
        self, start_addr: int, end_addr: int, min_size: int = 256
    ) -> List[Symbol]:
        """Find largest symbols within a range."""
        candidates = []
        for s in self.all_symbols:
            if s.start >= start_addr and (s.start + s.size) <= end_addr:
                if s.size >= min_size:
                    candidates.append(s)

        candidates.sort(key=lambda x: x.size, reverse=True)
        return candidates

    def has_stacks_in_range(self, start_addr: int, end_addr: int) -> bool:
        """Check if known stack symbols exist in this range."""
        stack_markers = [
            "z_interrupt_stacks",
            "z_main_stack",
            "_k_thread_stack",
        ]
        for s in self.all_symbols:
            if s.start >= start_addr and s.start < end_addr:
                for marker in stack_markers:
                    if marker in s.name:
                        return True
        return False


def analyze_ram(info: BuildInfo, elf_path: Path) -> Optional[Dict[str, Any]]:
    """Analyze RAM layout and return data structure."""
    # --- 1. RAM Configuration ---
    ram_base = info.get_symbol("CONFIG_SRAM_BASE_ADDRESS")
    ram_size_kb = info.get_symbol("CONFIG_SRAM_SIZE")

    if ram_base is not None and ram_size_kb is not None:
        ram_size = ram_size_kb * 1024
    else:
        ram_base = info.get_symbol("CONFIG_CROS_EC_RAM_BASE")
        ram_size = info.get_symbol("CONFIG_CROS_EC_RAM_SIZE")

    if ram_base is None or ram_size is None:
        print(
            f"Error: Could not determine RAM configuration for {elf_path}",
            file=sys.stderr,
        )
        return None

    ram_end = ram_base + ram_size

    # --- 2. Section Markers ---
    sections = []

    # Text in RAM (ILM)
    ilm_start = info.get_symbol("__ilm_ram_start")
    ilm_end = info.get_symbol("__ilm_ram_end")
    if ilm_start and ilm_end:
        sections.append(Section("ilm", ilm_start, ilm_end, False))

    # BSS
    bss_start = info.get_symbol("__bss_start")
    bss_end = info.get_symbol("__bss_end")
    if bss_start and bss_end:
        sections.append(Section("bss", bss_start, bss_end, False))

    # Data
    data_start = info.get_symbol("__data_start")
    data_end = info.get_symbol("__data_end")
    if data_start and data_end:
        sections.append(Section("data", data_start, data_end, False))

    # NoInit Logic (Explicit)
    if bss_end and data_start and data_start > bss_end:
        sections.append(Section("noinit", bss_end, data_start, False))

    # H2RAM Pool
    h2ram_start = info.get_symbol("_h2ram_pool_start")
    h2ram_end = info.get_symbol("_h2ram_pool_end")
    if h2ram_start and h2ram_end:
        sections.append(Section("h2ram_pool", h2ram_start, h2ram_end, False))

    # NoInit End of RAM
    noinit_end_start = info.get_symbol("__noinit_end_of_ram_start")
    noinit_end_end = info.get_symbol("__noinit_end_of_ram_end")
    if noinit_end_start and noinit_end_end:
        sections.append(
            Section(
                "noinit_end_of_ram", noinit_end_start, noinit_end_end, False
            )
        )

    # Preserved RAM
    preserved_size = info.get_symbol(
        "CONFIG_PLATFORM_EC_PRESERVED_END_OF_RAM_SIZE", 0
    )
    preserved_start = ram_end - preserved_size
    if preserved_size > 0:
        sections.append(
            Section("preserved_end_of_ram", preserved_start, ram_end, False)
        )

    # Sort sections
    sections.sort(key=lambda x: x.start)

    # --- 3. Calculations ---
    total_used = 0
    image_end = ram_base
    for s in sections:
        if s.name != "preserved_end_of_ram":
            image_end = max(image_end, s.end)
            total_used += s.end - s.start

    # Calculate unused regions
    final_regions = []
    current_addr = ram_base

    # Check for gap at start
    if sections and sections[0].start > ram_base:
        final_regions.append(
            Section("<UNUSED>", ram_base, sections[0].start, True)
        )

    for s in sections:
        # Check gap before this section
        if s.start > current_addr:
            gap_start = current_addr
            gap_end = s.start
            gap_name = "<UNUSED>"
            is_unused = True

            # Heuristic: Check if this "unused" gap actually contains stacks
            if info.has_stacks_in_range(gap_start, gap_end):
                gap_name = "noinit (Stacks)"
                is_unused = False
                total_used += gap_end - gap_start

            final_regions.append(
                Section(gap_name, gap_start, gap_end, is_unused)
            )

        final_regions.append(s)
        current_addr = s.end

    # Check gap at end if we haven't reached RAM end
    if current_addr < ram_end:
        if not (sections and sections[-1].name == "preserved_end_of_ram"):
            final_regions.append(
                Section("<UNUSED>", current_addr, ram_end, True)
            )

    return {
        "title": f"RAM Analysis for: {elf_path.name}",
        "type": "RAM",
        "base_addr": ram_base,
        "total_size": ram_size,
        "total_used": total_used,
        "regions": final_regions,
    }


def analyze_flash(info: BuildInfo, elf_path: Path) -> Optional[Dict[str, Any]]:
    """Analyze Flash layout and return data structure."""
    # --- 1. Flash Configuration ---
    flash_base = info.get_symbol("__rom_region_start")

    # Prefer CONFIG_FLASH_SIZE if available for total capacity
    flash_size_kb = info.get_symbol("CONFIG_FLASH_SIZE")
    if flash_size_kb:
        # Assume flash is split into RO/RW slots, so usable size is half
        total_flash_size = (flash_size_kb * 1024) // 2
    else:
        # Fallback to image size if total not known
        flash_end_image = info.get_symbol("__rom_region_end")
        if flash_base is None or flash_end_image is None:
            print(
                f"Warning: Could not determine Flash configuration for "
                f"{elf_path.name}",
                file=sys.stderr,
            )
            return None
        total_flash_size = flash_end_image - flash_base

    flash_end_chip = flash_base + total_flash_size

    # --- 2. Section Markers ---
    sections = []

    # Text Section
    text_start = info.get_symbol("__text_region_start")
    text_end = info.get_symbol("__text_region_end")
    if text_start and text_end:
        sections.append(Section("text", text_start, text_end, False))

    # RO Data Section
    rodata_start = info.get_symbol("__rodata_region_start")
    rodata_end = info.get_symbol("__rodata_region_end")
    if rodata_start and rodata_end:
        sections.append(Section("rodata", rodata_start, rodata_end, False))

    # Data LMA (Load Memory Address)
    data_lma_start = info.get_symbol("__data_region_load_start")
    data_size = info.get_symbol("__data_size")
    if data_lma_start and data_size:
        data_lma_end = data_lma_start + data_size
        sections.append(
            Section("data (LMA)", data_lma_start, data_lma_end, False)
        )

    sections.sort(key=lambda x: x.start)

    # --- 3. Calculations ---
    total_used = 0
    final_regions = []
    current_addr = flash_base

    # Filter out sections that are outside of the reported region (validity check)
    # or handle the fact that sections might not start at 0 offset if they are offset in flash.

    for s in sections:
        # Check gap before this section
        if s.start > current_addr:
            final_regions.append(
                Section("<UNUSED>", current_addr, s.start, True)
            )

        final_regions.append(s)
        total_used += s.end - s.start
        current_addr = s.end

    # Check gap at end (Tail Free)
    if current_addr < flash_end_chip:
        final_regions.append(
            Section("<UNUSED>", current_addr, flash_end_chip, True)
        )

    return {
        "title": f"Flash Analysis for: {elf_path.name}",
        "type": "Flash",
        "base_addr": flash_base,
        "total_size": total_flash_size,
        "total_used": total_used,
        "regions": final_regions,
    }


def print_text_report(data: Dict[str, Any], info: BuildInfo, threshold: int):
    """Print the formatted analysis report in text."""
    title = data["title"]
    base_addr = data["base_addr"]
    total_size = data["total_size"]
    total_used = data["total_used"]
    regions = data["regions"]

    print(title)
    print("=" * 80)
    print(f"{'Name':<40} {'Address':<12} {'Size (B)':<10} {'% Usage':<8}")
    print("=" * 80)

    total_avail_growth = 0
    regions.sort(key=lambda x: x.start)

    for r in regions:
        size = r.end - r.start
        pct = (size / total_size) * 100
        name = r.name

        if r.is_unused:
            total_avail_growth += size

        print(f"{name:<40} {hex(r.start):<12} {size:<10} {pct:5.1f}%")

        # Drill down if it's a used section and threshold is not disabled
        if not r.is_unused and size > 0 and threshold >= 0:
            top_syms = info.get_large_symbols(
                r.start, r.end, min_size=threshold
            )
            top_syms.sort(key=lambda x: x.size, reverse=True)
            if top_syms:
                for sym in top_syms:
                    sym_pct = (sym.size / total_size) * 100
                    print(
                        f"  {sym.name[:35]:<38} {'':<12} "
                        f"{sym.size:<10} {sym_pct:5.1f}%"
                    )
                print("")

    print("=" * 80)

    used_pct = (total_used / total_size) * 100
    avail_pct = (total_avail_growth / total_size) * 100

    print(f"{'Total':<40} {hex(base_addr):<12} {total_size:<10} 100.0%")
    print(f"{'Total Used':<40} {'':<12} {total_used:<10} {used_pct:5.1f}%")
    print(
        f"{'Total Available':<40} {'':<12} {total_avail_growth:<10} "
        f"{avail_pct:5.1f}%"
    )


def get_json_data(
    analysis_results: List[Dict[str, Any]],
    info_map: Dict[Path, BuildInfo],
    threshold: int,
) -> Dict[str, Any]:
    """Construct a dictionary for JSON output."""
    output = {"analyses": []}

    for data in analysis_results:
        # Lookup build info for this specific analysis
        elf_path = data.get("elf_path")
        info = info_map.get(elf_path)

        total_size = data["total_size"]
        analysis_out = {
            "type": data["type"],
            "title": data["title"],
            "base_address": hex(data["base_addr"]),
            "total_size_bytes": total_size,
            "total_used_bytes": data["total_used"],
            "sections": [],
        }

        for r in data["regions"]:
            sec = {
                "name": r.name,
                "start_address": hex(r.start),
                "end_address": hex(r.end),
                "size_bytes": r.end - r.start,
                "percent_usage": round(
                    ((r.end - r.start) / total_size) * 100, 2
                ),
                "is_unused": r.is_unused,
                "symbols": [],
            }

            if not r.is_unused and threshold >= 0 and info:
                top_syms = info.get_large_symbols(
                    r.start, r.end, min_size=threshold
                )
                top_syms.sort(key=lambda x: x.size, reverse=True)
                for sym in top_syms:
                    sec["symbols"].append(
                        {
                            "name": sym.name,
                            "start_address": hex(sym.start),
                            "size_bytes": sym.size,
                            "percent_usage": round(
                                (sym.size / total_size) * 100, 2
                            ),
                        }
                    )
            analysis_out["sections"].append(sec)

        output["analyses"].append(analysis_out)

    return output


def process_elf(
    elf_path: Path,
    analysis_type: str,
    analysis_results: List[Dict[str, Any]],
    info_map: Dict[Path, BuildInfo],
):
    """Process a single ELF file."""
    if not elf_path.exists():
        return

    info = BuildInfo(elf_path)
    info_map[elf_path] = info

    if analysis_type in ("ram", "both"):
        data = analyze_ram(info, elf_path)
        if data:
            data["elf_path"] = elf_path
            analysis_results.append(data)

    if analysis_type in ("flash", "both"):
        data = analyze_flash(info, elf_path)
        if data:
            data["elf_path"] = elf_path
            analysis_results.append(data)


def main():
    """Main function."""
    parser = argparse.ArgumentParser(
        description="Analyze Zephyr memory/flash layout"
    )
    parser.add_argument(
        "elf", type=Path, help="Path to Zephyr ELF file or packed bin"
    )
    parser.add_argument(
        "--type",
        choices=["ram", "flash", "both"],
        default="both",
        help="Type of analysis to perform (default: both)",
    )
    parser.add_argument(
        "--threshold",
        type=int,
        default=256,
        help="Minimum size in bytes to display a symbol in breakdown "
        "(default: 256). Set to -1 to disable symbol output.",
    )
    parser.add_argument(
        "--json", action="store_true", help="Output in JSON format"
    )
    args = parser.parse_args()

    analysis_results = []
    info_map = {}

    target_files = []

    if args.elf.suffix == ".bin":
        # Packed binary mode: look for siblings
        base_dir = args.elf.parent
        ro_elf = base_dir / "zephyr.ro.elf"
        rw_elf = base_dir / "zephyr.rw.elf"

        if ro_elf.exists():
            target_files.append(ro_elf)
        if rw_elf.exists():
            target_files.append(rw_elf)

        if not target_files:
            print(
                f"Error: Packed binary detected but could not find "
                f"'zephyr.ro.elf' or 'zephyr.rw.elf' in {base_dir}",
                file=sys.stderr,
            )
            sys.exit(1)
    else:
        # Single ELF mode
        if not args.elf.exists():
            print(f"Error: File {args.elf} not found.", file=sys.stderr)
            sys.exit(1)
        target_files.append(args.elf)

    for elf_file in target_files:
        process_elf(
            elf_file,
            args.type,
            analysis_results,
            info_map,
        )

    if args.json:
        json_out = get_json_data(analysis_results, info_map, args.threshold)
        print(json.dumps(json_out, indent=2))
    else:
        for i, data in enumerate(analysis_results):
            # Lookup correct build_info
            elf_path_key = data.get("elf_path")
            info = info_map.get(elf_path_key)

            print_text_report(data, info, args.threshold)
            if i < len(analysis_results) - 1:
                print()


if __name__ == "__main__":
    main()

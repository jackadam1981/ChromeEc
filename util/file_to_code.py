#!/usr/bin/env python3

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert a file to a reasonable code usable equivalent."""

import argparse
import mimetypes
import subprocess
import os
import re
import string
import sys
import textwrap
from datetime import date
from pathlib import Path
from typing import List, Optional, Tuple

# import PIL.Image as Image


def is_c_var(name: str) -> bool:
    """Check if the name can be a valid C variable name."""
    return not re.match("[a-zA-Z_][a-zA-Z0-9_]*", name) is None


class HeaderSourceContent:
    """Interface for generating header and source content."""

    @staticmethod
    def _scoped_name(
        name_prefix: str, file_path: Path, strip_path: Optional[Path] = None
    ) -> str:
        assert is_c_var(name_prefix)

        path = file_path
        if strip_path:
            path = file_path.relative_to(strip_path)
        name = name_prefix + (
            str(path).replace("/", "_").replace(".", "_").replace("-", "_")
        )
        assert is_c_var(name)
        return name

    @staticmethod
    def header_includes(name_prefix: str) -> List[str]:
        """Header file includes."""
        return []

    def header_pre_declarations(self, name_prefix: str) -> List[str]:
        """Content destined for the header pre-declaration section."""
        return []

    def header_declarations(self, name_prefix: str) -> List[str]:
        """Content destined for the header declarations section."""
        return []

    @staticmethod
    def source_includes(name_prefix: str) -> List[str]:
        """Source file includes."""
        return []

    def source_definitions(self, name_prefix: str) -> List[str]:
        """Content destined for the source file definitions section."""
        return []


class AnyFileToBytes(HeaderSourceContent):
    """Convert any binary file into C raw byte arrays."""

    def __init__(self, file_path: Path, strip_path: Optional[Path]) -> None:
        self._path = file_path
        self._strip = strip_path
        self._file_bytes_cache: Optional[bytes] = None

    def _file_bytes(self) -> bytes:
        if not self._file_bytes_cache:
            with open(self._path, "rb") as file:
                self._file_bytes_cache = file.read()
        return self._file_bytes_cache

    @staticmethod
    def _bytes_to_c(buf: bytes) -> str:
        return "{" + ", ".join(f"0x{b:02X}" for b in buf) + "}"

    def header_pre_declarations(self, name_prefix: str) -> List[str]:
        """Declarations that should be placed in the pre-declaration section."""
        return []

    def header_declarations(self, name_prefix: str) -> List[str]:
        name = self._scoped_name(name_prefix + "_", self._path, self._strip)
        buf = self._file_bytes()
        c_bytes = self._bytes_to_c(buf)
        return [
            f"#define {name.upper()} {c_bytes}",
            f"extern const char {name}[{len(buf)}];",
        ]

    def source_definitions(self, name_prefix: str) -> List[str]:
        name = self._scoped_name(name_prefix + "_", self._path, self._strip)
        buf_len = len(self._file_bytes())
        return [f"const char {name}[{buf_len}] = {name.upper()};"]


class TOC(HeaderSourceContent):
    """Inserts a table of contents file lookup."""

    def __init__(
        self, file_paths: List[Path], strip_path: Optional[Path]
    ) -> None:
        self._paths = file_paths
        self._strip = strip_path

    @staticmethod
    def header_includes(name_prefix: str) -> List[str]:
        return ["#include <stddef.h> // size_t"]

    def header_pre_declarations(self, name_prefix: str) -> List[str]:
        return [
            textwrap.dedent(
                """\
                struct data_toc {
                    const char *name;
                    const void *data;
                    size_t size;
                };
                """
            )
        ]

    def header_declarations(self, name_prefix: str) -> List[str]:
        # One extra item to hold sentinel.
        toc_length = len(self._paths) + 1
        return [
            textwrap.dedent(
                f"""
                extern const struct data_toc {name_prefix}[{toc_length}];
                """
            )
        ]

    @staticmethod
    def source_includes(name_prefix: str) -> List[str]:
        return ["#include <stddef.h> // NULL"]

    def source_definitions(self, name_prefix: str) -> List[str]:
        toc_length = len(self._paths) + 1
        defs: List[str] = []
        defs.append("\n")
        defs.append(
            f"const struct data_toc {name_prefix}[{toc_length}] = " + "{"
        )
        for path in self._paths:
            name = self._scoped_name(name_prefix + "_", path, self._strip)
            defs.append("{" + f'"{path}", &{name}, sizeof({name})' + "},")
        defs.append("{NULL, NULL, 0},")
        defs.append("};")

        return defs


class OutputFilesGenerator:
    """Generates the header and source files from HeaderSourceContent items."""

    TEMPLATE_HEADER_FILE = string.Template(
        textwrap.dedent(
            """\
            /* Copyright ${year} The ChromiumOS Authors
            * Use of this source code is governed by a BSD-style license that can be
            * found in the LICENSE file.
            */

            #ifndef __CROS_EC_${header_guard_full_path}
            #define __CROS_EC_${header_guard_full_path}

            ${includes}

            #ifdef __cplusplus
            extern "C" {
            #endif

            ${pre_declarations}
            ${declarations}

            #ifdef __cplusplus
            }
            #endif

            #endif /* __CROS_EC_${header_guard_full_path} */
            """
        )
    )

    TEMPLATE_SOURCE_FILE = string.Template(
        textwrap.dedent(
            """\
            /* Copyright ${year} The ChromiumOS Authors
            * Use of this source code is governed by a BSD-style license that can be
            * found in the LICENSE file.
            */

            ${includes}

            ${definitions}
            """
        )
    )

    @staticmethod
    def _gen_header_guard(ec_relative_header_path: Path) -> str:
        """Generate a full path header guard name."""

        assert len(ec_relative_header_path.parts) > 0
        assert not ec_relative_header_path.parts[0] in [".", "./", "/"]

        header_guard = (
            str(ec_relative_header_path)
            .replace("/", "_")
            .replace(".", "_")
            .upper()
        )

        assert is_c_var(header_guard)

        return header_guard

    def __init__(self) -> None:
        self._content: List[HeaderSourceContent] = []

    def add_content(self, content_item: HeaderSourceContent) -> None:
        """Add in data file to be converted."""
        self._content.append(content_item)

    def generate(
        self,
        output_dir: Path,
        name: str,
    ) -> Tuple[Path, Path]:
        """Generate the source and header files."""

        assert output_dir.exists()
        assert is_c_var(name)

        output_header_path = output_dir / (name + ".h")
        output_source_path = output_dir / (name + ".c")
        copyright_year = str(date.today().year)

        hdr_includes: List[str] = []
        hdr_pre_declarations: List[str] = []
        hdr_declarations: List[str] = []
        src_includes: List[str] = []
        src_definitions: List[str] = []

        for c_item in self._content:
            hdr_includes.extend(c_item.header_includes(name))
            hdr_pre_declarations.extend(c_item.header_pre_declarations(name))
            hdr_declarations.extend(c_item.header_declarations(name))
            src_includes.extend(c_item.source_includes(name))
            src_definitions.extend(c_item.source_definitions(name))

        hdr_includes = sorted(set(hdr_includes))
        src_includes = sorted(set(src_includes))
        # Source file will include header file.
        src_includes.insert(0, f'#include "{name}.h"\n')

        hdr = self.TEMPLATE_HEADER_FILE.substitute(
            {
                "year": copyright_year,
                "header_guard_full_path": self._gen_header_guard(
                    output_header_path
                ),
                "includes": "\n".join(hdr_includes),
                "pre_declarations": "\n".join(hdr_pre_declarations),
                "declarations": "\n".join(hdr_declarations),
            }
        )

        source = self.TEMPLATE_SOURCE_FILE.substitute(
            {
                "year": copyright_year,
                "includes": "\n".join(src_includes),
                "definitions": "\n".join(src_definitions),
            }
        )

        with open(output_header_path, "w") as c_item:
            c_item.write(hdr)

        with open(output_source_path, "w") as c_item:
            c_item.write(source)

        return (output_header_path, output_source_path)


class ImageToValues(AnyFileToBytes):
    # def __init__(self, png_file: Path) -> None:
    #     self._path = png_file

    def _file_bytes(self) -> bytes:
        # Image.fromqimage()
        return bytes("testeroo", "utf-8")

    @staticmethod
    def includes() -> List[str]:
        """Header files to include."""
        return []


REGISTERED_DECOMPOSERS = {
    "raw": AnyFileToBytes,
}

REGISTERED_MIMETYPES = {
    "image/jpeg": ImageToValues,
    "image/png": ImageToValues,
}


def main(argv: List[str]) -> int:
    """Detect the file mimetype and construct a reasonable source."""

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--outdir",
        default=Path("."),
        type=Path,
        help="destination source file we will write out",
    )
    parser.add_argument(
        "--strip",
        default=Path("."),
        type=Path,
        help="the root path to strip from in_files",
    )
    parser.add_argument(
        "--name",
        default="data",
        help="file and symbols name prefixes to use",
    )
    parser.add_argument(
        "--toc",
        action="store_true",
        help="include a table of contents to lookup files by path",
    )
    parser.add_argument(
        "--clang-format",
        action="store_true",
        help="run clang-format on the output files",
    )
    parser.add_argument(
        "--test",
        action="store_true",
        help="test the output files by compiling them",
    )
    parser.add_argument(
        "in_files",
        nargs="+",
        help="source file(s) we will read in with optional colon decomposition specifier",
    )
    args = parser.parse_args(argv)

    if not is_c_var(args.name):
        parser.error(f"name arg '{args.name}' is not a valid C variable name")

    out = OutputFilesGenerator()

    for file in args.in_files:
        decomposer = AnyFileToBytes
        parts = file.split(":", 2)
        file_path = parts[0]

        if len(parts) == 1:
            mime = mimetypes.guess_type(file)
            if mime in REGISTERED_MIMETYPES:
                decomposer = REGISTERED_DECOMPOSERS[mime]
        elif len(parts) == 2:
            if parts[1] not in REGISTERED_DECOMPOSERS:
                parser.error(
                    f"Error - decomposition '{parts[1]}' does not exist"
                )
            decomposer = REGISTERED_DECOMPOSERS[parts[1]]

        rel_path = Path(os.path.relpath(file_path))
        out.add_content(decomposer(rel_path, args.strip))

    if args.toc:
        out.add_content(TOC([Path(f) for f in args.in_files], args.strip))

    out_hdr_path, out_src_path = out.generate(args.outdir, args.name)

    if args.clang_format:
        cmd = [
            "clang-format",
            "-i",
            out_hdr_path,
            out_src_path,
        ]
        subprocess.run(cmd, check=True)

    if args.test:
        output_object = Path("/tmp/file-to-code-test.o")
        if output_object.exists():
            output_object.unlink()
        cmd = [
            "cc",
            "-Wall",
            "-o",
            str(output_object),
            "-c",
            out_src_path,
        ]
        subprocess.run(cmd, check=True)
        if not output_object.exists():
            print("Error - Could not find test compilation output object")
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

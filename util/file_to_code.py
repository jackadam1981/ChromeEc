#!/usr/bin/env python3

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert a file to a reasonable code usable equivalent."""

import argparse
import mimetypes
import os
import string
import sys
import textwrap
from datetime import date
from pathlib import Path
from typing import List, Optional

import PIL.Image as Image


class AnyToBytes:
    def __init__(self, file_path: Path) -> None:
        self._path = file_path
        self._file_bytes_cache: Optional[bytes] = None

    def _scoped_name(self) -> str:
        return "filedata_" + (
            str(self._path)
            .replace("/", "_")
            .replace(".", "_")
            .replace("-", "_")
        )

    def _file_bytes(self) -> bytes:
        if not self._file_bytes_cache:
            with open(self._path, "rb") as file:
                self._file_bytes_cache = file.read()
        return self._file_bytes_cache

    @staticmethod
    def _bytes_to_c(buf: bytes) -> str:
        return "{" + ", ".join(f"0x{b:02X}" for b in buf) + "}"

    @staticmethod
    def header_includes() -> List[str]:
        """Header file includes."""
        return []

    def header_pre_declarations(self) -> List[str]:
        """Declarations that should be placed in the pre-declaration section."""
        return []

    def header_declarations(self) -> List[str]:
        buf = self._file_bytes()
        c_bytes = self._bytes_to_c(buf)
        return [
            f"#define {self._scoped_name().upper()} {c_bytes}",
            f"extern const char {self._scoped_name()}[{len(buf)}];",
        ]

    @staticmethod
    def source_includes() -> List[str]:
        """Source file includes."""
        return []

    def source_definitions(self) -> List[str]:
        buf = self._file_bytes()
        c_bytes = self._bytes_to_c(buf)
        return [
            f"const char {self._scoped_name()}[{len(buf)}] = {c_bytes};",
        ]


class OutputFilesGenerator:
    TEMPLATE_HEADER_FILE = string.Template(
        textwrap.dedent(
            """
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
            """
            /* Copyright ${year} The ChromiumOS Authors
            * Use of this source code is governed by a BSD-style license that can be
            * found in the LICENSE file.
            */

            ${includes}

            ${definitions}
            """
        )
    )

    TEMPLATE_TOC = string.Template(
        textwrap.dedent(
            """
            #include <stddef.h>
            struct data_header {
                size_t size;
                char data[];
            };

            struct data_toc {
                char *name;
                size_t size;
                void *data;
            };
            """
        )
    )

    @staticmethod
    def _gen_header_guard(ec_relative_header_path: Path) -> str:
        """Generate a full path header guard name."""

        assert len(ec_relative_header_path.parts) > 0
        assert not ec_relative_header_path.parts[0] in [".", "./", "/"]

        return str(ec_relative_header_path).replace("/", "_").upper()

    def __init__(self) -> None:
        # self._output_dir = output_dir
        self._files: List[AnyToBytes] = []

    def add_file(self, file: AnyToBytes) -> None:
        self._files.append(file)

    def generate(self, output_dir: Path):

        copyright_year = str(date.today().year)

        hdr_includes = ["\n".join(f.header_includes()) for f in self._files]
        hdr_pre_declarations = [
            "\n".join(f.header_pre_declarations()) for f in self._files
        ]
        hdr_declarations = [
            "\n".join(f.header_declarations()) for f in self._files
        ]

        hdr = self.TEMPLATE_HEADER_FILE.substitute(
            {
                "year": copyright_year,
                "header_guard_full_path": self._gen_header_guard(output_dir),
                "includes": "\n".join(hdr_includes),
                "pre_declarations": "\n".join(hdr_pre_declarations),
                "declarations": "\n".join(hdr_declarations),
            }
        )

        src_includes = ["\n".join(f.source_includes()) for f in self._files]
        src_definitions = [
            "\n".join(f.source_definitions()) for f in self._files
        ]

        source = self.TEMPLATE_SOURCE_FILE.substitute(
            {
                "year": copyright_year,
                "includes": "\n".join(src_includes),
                "definitions": "\n".join(src_definitions),
            }
        )

        with open(output_dir / "header.h", "w") as file:
            file.write(hdr)

        with open(output_dir / "source.c", "w") as file:
            file.write(source)


class ImageToValues(AnyToBytes):
    # def __init__(self, png_file: Path) -> None:
    #     self._path = png_file

    def _file_bytes(self) -> bytes:
        # Image.fromqimage()
        return bytes("testeroo", "utf-8")

    @staticmethod
    def includes() -> List[str]:
        """Header files to include."""
        return []


def main(argv: List[str]) -> int:
    """Detect the file mimetype and construct a reasonable source."""

    parser = argparse.ArgumentParser(description=__doc__)
    # There are far too many boards to show in help message, so we omit the
    # choice option and manually check the boards below.
    parser.add_argument(
        "--out_dir",
        nargs="?",
        default=".",
        help="destination source file we will write out",
    )
    parser.add_argument(
        "in_files", nargs="+", help="source file we will read in for conversion"
    )
    parser.add_argument("--symbol_name", nargs="?", help="")
    args = parser.parse_args(argv)

    registered_decomposers = {
        "image/jpeg": ImageToValues,
        "image/png": ImageToValues,
    }

    ec_root = Path(os.path.relpath(os.path.dirname(__file__) + "/.."))

    out = OutputFilesGenerator()

    for file in args.in_files:
        rel_path = Path(os.path.relpath(file))
        out.add_file(AnyToBytes(rel_path))
        # mime = mimetypes.guess_type(f)
        # if mime in registered_decomposers:
        #     obj = registered_decomposers[mime](f)
        # else:
        #     obj = AnyToBytes(f)
        # print(obj.bytes())

    out.generate(Path("outdir"))

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

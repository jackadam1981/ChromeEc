#!/usr/bin/env python3

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert a file to a reasonable code usable equivalent."""

import argparse
import mimetypes
import os
import re
import string
import subprocess
import sys
import textwrap
from datetime import date
from pathlib import Path
from typing import List, Optional, Tuple

import PIL.Image as Image
from more_itertools import strip


def is_c_var(name: str) -> bool:
    """Check if the name can be a valid C variable name."""
    return not re.match("[a-zA-Z_][a-zA-Z0-9_]*", name) is None


class FileDecompositionFormat:
    """Parses the "file:decomposition" specifier format given as in_files.

    Example specifiers: testdata/image1.png:raw

    Attributes:
        path: The path component of the specifier.
        decomp: The optional decomposition part of the specifier.
    """

    path: Path
    decomp: Optional[str]

    def __init__(self, file_and_conversion_str: str) -> None:
        parts = file_and_conversion_str.split(":", 2)
        assert len(parts) == 1 or len(parts) == 2
        self.path = Path(parts[0])
        self.decomp = None
        if len(parts) == 2:
            self.decomp = parts[1]


class HeaderSourceContent:
    """Interface for generating header and source content."""

    @classmethod
    def _striped_path(cls, file_path: Path, strip_path: Optional[Path] = None):
        """Return file_path without the leading strip_path."""
        if strip_path:
            return file_path.relative_to(strip_path)
        return file_path

    @classmethod
    def _scoped_name(
        cls,
        ns_prefix: str,
        file_path: Path,
        strip_path: Optional[Path] = None,
    ) -> str:
        assert is_c_var(ns_prefix)

        path = cls._striped_path(file_path, strip_path)
        name = ns_prefix + (
            str(path).replace("/", "_").replace(".", "_").replace("-", "_")
        )
        assert is_c_var(name)
        return name

    @staticmethod
    def _bytes_to_c(buf: bytes) -> str:
        return "{" + ", ".join(f"0x{b:02X}" for b in buf) + "}"

    @staticmethod
    def header_includes(ns_prefix: str) -> List[str]:
        """Header file includes."""
        return []

    def header_pre_declarations(self, ns_prefix: str) -> List[str]:
        """Content destined for the header pre-declaration section.

        Declarations here will be deduplicated while preserving order.
        """
        return []

    def header_declarations(self, ns_prefix: str) -> List[str]:
        """Content destined for the header declarations section."""
        return []

    @staticmethod
    def source_includes(ns_prefix: str) -> List[str]:
        """Source file includes."""
        return []

    def source_definitions(self, ns_prefix: str) -> List[str]:
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
    def header_includes(ns_prefix: str) -> List[str]:
        return [
            "#include <stdint.h>",  # uint8_t
        ]

    def header_declarations(self, ns_prefix: str) -> List[str]:
        path = self._striped_path(self._path, self._strip)
        name = self._scoped_name(ns_prefix + "_", self._path, self._strip)
        buf = self._file_bytes()
        c_bytes = self._bytes_to_c(buf)
        return [
            f"// Decomposition of {path} using raw.",
            f"#define {name.upper()}_BYTES {c_bytes}",
            f"// Decomposition of {path} using raw.",
            f"extern const uint8_t {name}[{len(buf)}];",
        ]

    @staticmethod
    def source_includes(ns_prefix: str) -> List[str]:
        return [
            "#include <stdint.h>",  # uint8_t
        ]

    def source_definitions(self, ns_prefix: str) -> List[str]:
        path = self._striped_path(self._path, self._strip)
        name = self._scoped_name(ns_prefix + "_", self._path, self._strip)
        buf_len = len(self._file_bytes())
        return [
            f"// Decomposition of {path} using raw.",
            f"const uint8_t {name}[{buf_len}] = {name.upper()}_BYTES;",
        ]


class ImageToValues(HeaderSourceContent):

    TEMPLATE_PRE_DECLARATION = string.Template(
        textwrap.dedent(
            """\
            // This is the generic flat interface for all images.
            // It is assumed the bit depth is 8 bit.
            struct ${ns}_image {
                size_t height;
                size_t width;
                uint8_t channels;
                uint8_t data[];
            };

            // This is the multi-dimensional type proto for an image.
            #define ${NS}_IMAGE_MULTI_TYPE(rows,cols,chans)     \
                struct {                                        \
                    size_t height;                              \
                    size_t width;                               \
                    uint8_t channels;                           \
                    uint8_t data[rows][cols][chans];            \
                }
            """
        )
    )

    TEMPLATE_DECLARATION = string.Template(
        textwrap.dedent(
            """\
            // Decomposition of ${path} using img.
            #define ${NAME}_BYTES ${bytes}
            // Decomposition of ${path} using img.
            typedef ${NS}_IMAGE_MULTI_TYPE(${height},${width},${channels})
                ${name}_t;
            extern const ${name}_t ${name};
            """
        )
    )

    TEMPLATE_DEFINITION = string.Template(
        textwrap.dedent(
            """\
            // Decomposition of ${path} using img.
            const ${name}_t ${name} = {
                .height = ${height},
                .width = ${width},
                .channels = ${channels},
                .data = (uint8_t [${height}][${width}][${channels}]) ${NAME}_BYTES,
            };
            """
        )
    )

    def __init__(self, image_file: Path, strip_path: Optional[Path]) -> None:
        self._path = image_file
        self._strip = strip_path

        image = Image.open(image_file, "r")
        self._im_width = image.width
        self._im_height = image.height
        self._im_channels = len(image.getbands())
        self._im_bytes = image.tobytes(encoder_name="raw")
        image.close()

    @staticmethod
    def header_includes(ns_prefix: str) -> List[str]:
        """Header file includes."""
        return [
            "#include <stddef.h>",  # size_t
            "#include <stdint.h>",  # uint8_t
        ]

    def header_pre_declarations(self, ns_prefix: str) -> List[str]:
        return [
            self.TEMPLATE_PRE_DECLARATION.substitute(
                ns=ns_prefix, NS=ns_prefix.upper()
            )
        ]

    def header_declarations(self, ns_prefix: str) -> List[str]:
        path = self._striped_path(self._path, self._strip)
        name = self._scoped_name(ns_prefix + "_", self._path, self._strip)
        # buf = self._file_bytes()
        c_bytes = self._bytes_to_c(self._im_bytes)
        return [
            self.TEMPLATE_DECLARATION.substitute(
                ns=ns_prefix,
                NS=ns_prefix.upper(),
                name=name,
                NAME=name.upper(),
                path=path,
                bytes=c_bytes,
                height=self._im_height,
                width=self._im_width,
                channels=self._im_channels,
            ),
        ]

    @staticmethod
    def source_includes(ns_prefix: str) -> List[str]:
        return [
            "#include <stdint.h>",  # uint8_t
        ]

    def source_definitions(self, ns_prefix: str) -> List[str]:
        path = self._striped_path(self._path, self._strip)
        name = self._scoped_name(ns_prefix + "_", self._path, self._strip)
        # buf_len = len(self._file_bytes())
        return [
            self.TEMPLATE_DEFINITION.substitute(
                name=name,
                NAME=name.upper(),
                path=path,
                height=self._im_height,
                width=self._im_width,
                channels=self._im_channels,
            )
        ]


class TOC(HeaderSourceContent):
    """Inserts a table of contents file lookup."""

    TEMPLATE_TOC_STRUCT = string.Template(
        textwrap.dedent(
            """\
            struct ${ns}_toc {
                const char *name;
                const void *data;
                size_t size;
            };
            """
        )
    )

    def __init__(
        self, file_paths: List[Path], strip_path: Optional[Path]
    ) -> None:
        self._paths = file_paths
        self._strip = strip_path

    @staticmethod
    def header_includes(ns_prefix: str) -> List[str]:
        return [
            "#include <stddef.h>",  # size_t
        ]

    def header_pre_declarations(self, ns_prefix: str) -> List[str]:
        return [self.TEMPLATE_TOC_STRUCT.substitute(dict(ns=ns_prefix))]

    def header_declarations(self, ns_prefix: str) -> List[str]:
        # One extra item to hold sentinel.
        toc_length = len(self._paths) + 1
        return [
            textwrap.dedent(
                f"""
                extern const struct {ns_prefix}_toc {ns_prefix}_toc[{toc_length}];
                """
            )
        ]

    @staticmethod
    def source_includes(ns_prefix: str) -> List[str]:
        return [
            "#include <stddef.h>",  # NULL
        ]

    def source_definitions(self, ns_prefix: str) -> List[str]:
        toc_length = len(self._paths) + 1
        defs: List[str] = []
        defs.append("\n")
        defs.append(
            f"const struct {ns_prefix}_toc {ns_prefix}_toc[{toc_length}] = "
            + "{"
        )
        for path_full in self._paths:
            path = self._striped_path(path_full, self._strip)
            name = self._scoped_name(ns_prefix + "_", path_full, self._strip)
            defs.append("    {" + f'"{path}", &{name}, sizeof({name})' + "},")
        defs.append("    {NULL, NULL, 0},")
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
            .replace("-", "_")
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
        ns: str,
        output_dir: Path,
        file: Path,
    ) -> Tuple[Path, Path]:
        """Generate the source and header files."""

        assert output_dir.exists()
        assert is_c_var(ns)

        header_file_base = file.with_suffix(file.suffix + ".h")
        source_file_base = file.with_suffix(file.suffix + ".c")
        output_header_path = output_dir / header_file_base
        output_source_path = output_dir / source_file_base
        copyright_year = str(date.today().year)

        hdr_includes: List[str] = []
        hdr_pre_declarations: List[str] = []
        hdr_declarations: List[str] = []
        src_includes: List[str] = []
        src_definitions: List[str] = []

        for c_item in self._content:
            hdr_includes.extend(c_item.header_includes(ns))
            hdr_pre_declarations.extend(c_item.header_pre_declarations(ns))
            hdr_declarations.extend(c_item.header_declarations(ns))
            src_includes.extend(c_item.source_includes(ns))
            src_definitions.extend(c_item.source_definitions(ns))

        # Deduplicate pre-declarations while preserving order.
        hdr_pre_declarations = list(dict.fromkeys(hdr_pre_declarations))
        # Deduplicate and sort includes.
        hdr_includes = sorted(set(hdr_includes))
        src_includes = sorted(set(src_includes))
        # Include header file from source file.
        src_includes.insert(0, f'#include "{header_file_base}"\n')

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


REGISTERED_DECOMPOSERS = {
    "raw": AnyFileToBytes,
    "img": ImageToValues,
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
        "--file",
        default=Path("data"),
        type=Path,
        help="file name prefix to use for header and source file names",
    )
    parser.add_argument(
        "--ns",
        default="data",
        help="symbols name prefix",
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
        type=FileDecompositionFormat,
        help="source file(s) we will read in with optional colon decomposition specifier",
    )
    args = parser.parse_args(argv)

    if not is_c_var(args.ns):
        parser.error(f"ns arg '{args.ns}' is not a valid C variable name")

    out = OutputFilesGenerator()

    for file in args.in_files:
        file: FileDecompositionFormat
        decomposer = AnyFileToBytes

        if file.decomp:
            if file.decomp not in REGISTERED_DECOMPOSERS:
                parser.error(
                    f"Error - decomposition '{file.decomp}' does not exist"
                )
            decomposer = REGISTERED_DECOMPOSERS[file.decomp]
        else:
            mime, _ = mimetypes.guess_type(file.path)
            if mime in REGISTERED_MIMETYPES:
                decomposer = REGISTERED_MIMETYPES[mime]

        out.add_content(decomposer(file.path, args.strip))

    if args.toc:
        out.add_content(TOC([f.path for f in args.in_files], args.strip))

    out_hdr_path, out_src_path = out.generate(args.ns, args.outdir, args.file)

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

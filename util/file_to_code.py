#!/usr/bin/env python3

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert a file to a reasonable code usable equivalent."""

import argparse
import mimetypes
import os
from re import TEMPLATE
import string
import sys
import textwrap
import PIL.Image as Image
from pathlib import Path
from typing import List, Optional


class OutputFiles:

    TEMPLATE_DECLARATION = string.Template("const char[] = { ${bytes} };")

    TEMPLATE_HEADER_FILE = string.Template(
        textwrap.dedent(
            """
            /* Copyright 2022 The ChromiumOS Authors
            * Use of this source code is governed by a BSD-style license that can be
            * found in the LICENSE file.
            */

            #ifndef __CROS_EC_${header_guard_full_path}
            #define __CROS_EC_${header_guard_full_path}

            ${declarations}

            #endif /* __CROS_EC_${header_guard_full_path} */
            """
        )
    )

    TEMPLATE_SOURCE_FILE = string.Template(
        textwrap.dedent(
            """
            /* Copyright 2022 The ChromiumOS Authors
            * Use of this source code is governed by a BSD-style license that can be
            * found in the LICENSE file.
            */

            ${definitions}
            """
        )
    )

    @staticmethod
    def _gen_header_guard(ec_relative_header_path: Path) -> str:
        """Generate a full path header guard name."""

        assert len(ec_relative_header_path.parts) > 0
        assert not ec_relative_header_path.parts[0] in [".", "./", "/"]

        return str(ec_relative_header_path).replace("/", "_").upper()

    def __init__(self, output_dir: Path) -> None:
        self._output_dir = output_dir

    def generate(self):
        print(
            self.TEMPLATE_HEADER_FILE.substitute(
                {
                    "header_guard_full_path": self._gen_header_guard(
                        self._output_dir
                    ),
                    "declarations": "decls here",
                }
            )
        )


class AnyToBytes:
    def __init__(self, file_path: Path) -> None:
        self._path = file_path

    @staticmethod
    def includes() -> List[str]:
        """Header files to include."""
        return []

    def bytes(self) -> bytes:
        with open(self._path, "rb") as f:
            return f.read()


class ImageToValues(AnyToBytes):
    # def __init__(self, png_file: Path) -> None:
    #     self._path = png_file

    def bytes(self) -> bytes:
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

    for f in args.in_files:
        mime = mimetypes.guess_type(f)
        if mime in registered_decomposers:
            obj = registered_decomposers[mime](f)
        else:
            obj = AnyToBytes(f)
        print(obj.bytes())

    out = OutputFiles(Path("base/dis"))
    out.generate()

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env vpython3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks files for invalid CPRINTS calls."""

import argparse
import contextlib
import pathlib
import re
import subprocess
import sys
from typing import Generator, IO, List, Optional


PRE_SUBMIT = "pre-submit"
CPRINTS_RE = re.compile(r'(CPRINTS|cprints)[^"]*"[^"]*\\n"')


@contextlib.contextmanager
def cat_file(args, filename) -> Generator[IO[str], None, None]:
    """Read a file either from disk, or from a git commit."""
    if args.commit == PRE_SUBMIT:
        with open(filename, encoding="utf-8") as infile:
            yield infile
    else:
        with subprocess.Popen(
            ["git", "show", f"{args.commit}:{filename}"],
            universal_newlines=True,
            stdout=subprocess.PIPE,
        ) as cmd:
            assert cmd.stdout
            yield cmd.stdout
            if cmd.wait():
                raise subprocess.CalledProcessError(cmd.returncode, cmd.args)


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    """Look at all files passed in on commandline for invalid CPRINTS calls."""
    return_code = 0
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--commit", default=PRE_SUBMIT)
    parser.add_argument("filename", nargs="+", type=pathlib.Path)

    args = parser.parse_args(argv)
    for filename in args.filename:
        with cat_file(args, filename) as infile:
            for linenum, line in enumerate(infile, start=1):
                line = line.rstrip("\n")
                if CPRINTS_RE.search(line):
                    print(
                        "error: CPRINTS strings should not include newline "
                        f"characters\n{filename}:{linenum}: {line}",
                        file=sys.stderr,
                    )
                    return_code = 1

    return return_code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

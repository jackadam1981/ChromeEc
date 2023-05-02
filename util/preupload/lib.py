# Lint as: python3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities for pre-upload checks."""

import argparse
import contextlib
import pathlib
import subprocess
from typing import Generator, IO


PRE_SUBMIT = "pre-submit"


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


def argument_parser():
    """Returns an ArgumentParser with standard options configured."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--commit", default=PRE_SUBMIT)
    parser.add_argument("filename", nargs="+", type=pathlib.Path)
    return parser

#!/usr/bin/env vpython3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Do stuff."""

# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/pyyaml-py3"
#   version: "version:5.3.1"
# >
# [VPYTHON:END]

import argparse
from contextlib import contextmanager
import re
import subprocess
import sys
from typing import Generator, IO

import yaml  # pylint:disable=import-error


PRE_SUBMIT = "pre-submit"
CPRINTS_RE = re.compile(r'(CPRINTS|cprints)[^"]*"[^"]*\\n"')
ZTEST_RE = re.compile(
    r"(ZTEST|ZTEST_F|ZTEST_USER|ZTEST_USER_F)\(\w+,\s*($|\w+)"
)
BLOCKED_FIELDS = {
    "CONF_FILE": "extra_conf_files",
    "OVERLAY_CONFIG": "extra_overlay_confs",
    "DTC_OVERLAY_FILE": "extra_dtc_overlay_files",
}


@contextmanager
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


def main():
    """Do Stuff."""
    return_code = 0
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--commit", default=PRE_SUBMIT)
    parser.add_argument("filename", nargs="+")

    args = parser.parse_args()
    for filename in args.filename:
        with cat_file(args, filename) as infile:
            if filename.endswith("/testcase.yaml"):
                data = yaml.load(infile, Loader=yaml.SafeLoader)

                def scan_section(section, filename):
                    nonlocal return_code
                    if "extra_args" in section:
                        for field, replacement in BLOCKED_FIELDS.items():
                            if field in section["extra_args"]:
                                print(
                                    f"error: Don't specify {field} in "
                                    f"`extra_args`. Use `{replacement}`: "
                                    f"{filename}",
                                    file=sys.stderr,
                                )
                                return_code = 1

                if "common" in data:
                    scan_section(data["common"], filename)
                if "tests" in data:
                    for section in data["tests"]:
                        scan_section(data["tests"][section], filename)
                continue
            linenum = 0
            for line in infile:
                line = line.strip("\n")
                linenum += 1
                if CPRINTS_RE.search(line):
                    print(
                        "error: CPRINTS strings should not include newline "
                        f"characters\n{filename}:{linenum}: {line}",
                        file=sys.stderr,
                    )
                    return_code = 1
                match = ZTEST_RE.search(line)
                if match and match.group(2) == "":
                    linenum += 1
                    line2 = infile.readline()
                    line += line2.strip("\n")
                    match = ZTEST_RE.search(line)
                if match and not match.group(2).startswith("test_"):
                    print(
                        "error: 'test_' prefix missing from test function name"
                        f"\n{filename}:{linenum}: {match.group(2)}",
                        file=sys.stderr,
                    )
                    return_code = 1

    return return_code


if __name__ == "__main__":
    main()

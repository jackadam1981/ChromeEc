#!/usr/bin/env python3

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code to generate the ec_version.h file."""

import argparse
import datetime
import getpass
import io
import logging
import os
import pathlib
import platform
import subprocess
import sys

def _get_num_commits(repo):
    """Get the number of commits that have been made.

    If a Git repository is available, return the number of commits that have
    been made. Otherwise return a fixed count.

    Args:
        repo: The path to the git repo.

    Returns:
        An integer, the number of commits that have been made.
    """
    try:
        result = subprocess.run(
            ["git", "-C", repo, "rev-list", "HEAD", "--count"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
        )
    except subprocess.CalledProcessError:
        commits = "9999"
    else:
        commits = result.stdout

    return int(commits)


def _get_revision(repo):
    """Get the current revision hash.

    If a Git repository is available, return the hash of the current index.
    Otherwise return the hash of the VCSID environment variable provided by
    the packaging system.

    Args:
        repo: The path to the git repo.

    Returns:
        A string, of the current revision.
    """
    try:
        result = subprocess.run(
            ["git", "-C", repo, "log", "-n1", "--format=%H"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
        )
    except subprocess.CalledProcessError:
        # Fall back to the VCSID provided by the packaging system.
        # Format is 0.0.1-r425-032666c418782c14fe912ba6d9f98ffdf0b941e9 for
        # releases and 9999-032666c418782c14fe912ba6d9f98ffdf0b941e9 for
        # 9999 ebuilds.
        vcsid = os.environ.get("VCSID", "9999-unknown")
        revision = vcsid.rsplit("-", 1)[1]
    else:
        revision = result.stdout

    return revision

def _c_str(input_str):
    """Make a string that can be included as a literal in C source code.

    Args:
        input_str: The string to process.

    Returns:
        A string which can be included in C source code.
    """

    def c_chr(char):
        # Convert a char in a string to the C representation.  Per the
        # C standard, we can use all characters but quote, newline,
        # and backslash directly with no replacements.
        return {
            '"': r"\"",
            "\n": r"\n",
            "\\": "\\\\",
        }.get(char, char)

    return '"{}"'.format("".join(map(c_chr, input_str)))

def convert_module_list_to_dict(modules: list) -> dict:
    """Convert a list of string paths to modules in to a dict of module
    names to paths."""

    if not modules:
        return {}

    dict_out = {}
    for mod in modules:
        mod = mod.rstrip("/")

        if not os.path.isdir(mod):
            raise FileNotFoundError(f"Module '{mod}' not found")

        dict_out[os.path.basename(mod)] = mod

    return dict_out

def read_zephyr_version(zephyr_base):
    """Read the Zephyr version from a Zephyr OS checkout.

    Args:
         zephyr_base: path to the Zephyr OS repository.

    Returns:
         A 3-tuple of the version number (major, minor, patchset).
    """
    version_file = pathlib.Path(zephyr_base) / "VERSION"

    file_vars = {}
    with open(version_file) as file:
        for line in file:
            key, _, value = line.partition("=")
            file_vars[key.strip()] = value.strip()

    return (
        int(file_vars["VERSION_MAJOR"]),
        int(file_vars["VERSION_MINOR"]),
        int(file_vars["PATCHLEVEL"]),
    )

def get_version_string(project_name, zephyr_base, modules, static=False):
    """Get the version string associated with a build.

    Args:
        project_name: string name of project
        zephyr_base: the path to the zephyr directory
        modules: a dictionary mapping module names to module paths
        static: if set, create a version string not dependent on git
            commits, thus allowing binaries to be compared between two
            commits.

    Returns:
        A version string which can be placed in FRID, FWID, or used in
        the build for the OS.
    """
    major_version, minor_version, *_ = read_zephyr_version(zephyr_base)
    num_commits = 0

    if static:
        vcs_hashes = "STATIC"
    else:
        repos = {
            "os": zephyr_base,
            **modules,
        }

        for repo in repos.values():
            num_commits += _get_num_commits(repo)

        vcs_hashes = ",".join(
            "{}:{}".format(name, _get_revision(repo)[:6])
            for name, repo in sorted(
                repos.items(),
                # Put the EC module first, then Zephyr OS kernel, as
                # these are probably the most important hashes to
                # developers.
                key=lambda p: (p[0] != "ec", p[0] != "os", p),
            )
        )

    return "{}_v{}.{}.{}-{}".format(
        project_name,
        major_version,
        minor_version,
        num_commits,
        vcs_hashes,
    )

def write_version_header(version_str, output_path, static=False):
    """Generate a version header and write it to the specified path.

    Generate a version header in the format expected by the EC build
    system, and write it out only if the version header does not exist
    or changes.  We don't write in the case that the version header
    does exist and was unchanged, which allows "zmake build" commands
    on an unchanged tree to be an effective no-op.

    Args:
        version_str: The version string to be used in the header, such
            as one generated by get_version_string.
        output_path: The file path to write at (a pathlib.Path
            object).
        static: If true, generate a header which does not include
            information like the username, hostname, or date, allowing
            the build to be reproducible.
    """
    output = io.StringIO()
    output.write("/* This file is automatically generated by zmake */\n")

    def add_def(name, value):
        output.write("#define {} {}\n".format(name, _c_str(value)))

    def add_def_unquoted(name, value):
        output.write("#define {} {}\n".format(name, value))

    add_def("VERSION", version_str)
    add_def("CROS_EC_VERSION32", version_str[:31])

    if static:
        add_def("BUILDER", "reproducible@build")
        add_def("DATE", "STATIC_VERSION_DATE")
    else:
        add_def("BUILDER", "{}@{}".format(getpass.getuser(), platform.node()))
        add_def("DATE", datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

    add_def("CROS_FWID_MISSING_STR", "CROS_FWID_MISSING")
    # TODO(b/198475757): Add zmake support for getting CROS_FWID32
    add_def_unquoted("CROS_FWID32", "CROS_FWID_MISSING_STR")

    # Create all directories in the path
    output_path.parent.mkdir(parents=True, exist_ok=True)

    contents = output.getvalue()
    if not output_path.exists() or output_path.read_text() != contents:
        output_path.write_text(contents)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, stream=sys.stdout)

    parser = argparse.ArgumentParser()

    parser.add_argument(
        "header_path",
        help="Path to write ec_version.h to"
    )
    parser.add_argument(
        "-s", "--static",
        action="store_true",
        help="If set, generate a header which does not include information " +
        "like the username, hostname, or date, allowing the build to be" +
        "reproducible."
    )
    parser.add_argument(
        "--base",
        default=os.environ.get("ZEPHYR_BASE"),
        help="Path to Zephyr base directory. Uses ZEPHYR_BASE env var if unset."
    )
    parser.add_argument(
        "-m", "--module",
        action="append",
        help="Specify modules paths to include in version hash."
    )
    parser.add_argument(
        "-n", "--name", required=True, type=str, help="Project name"
    )

    args = parser.parse_args()

    if args.base is None:
        logging.error("No Zephyr base is defined. Pass --base or set env var ZEPHYR_BASE")
        sys.exit(1)

    logging.info("Zephyr Base: %s", args.base)

    # Make a dict of modules from the list. Modules can be added one at a time
    # by repeating the -m flag, or once as a semicolon-separated list. In the
    # later case, we need to expand the modules list.

    if args.module is None:
        args.module = []
    elif len(args.module) == 1:
        args.module= args.module[0].split(";")

    try:
        module_dict = convert_module_list_to_dict(args.module)
    except FileNotFoundError as err:
        logging.error("Error locating module: %s", str(err))
        sys.exit(1)

    logging.info("Including modules: [%s]", ", ".join(args.module))

    # pylint:disable=invalid-name

    # Generate the version string that gets inserted in to the header. Will get
    # commit IDs from Git
    ver = get_version_string(
        args.name, args.base, module_dict, args.static
    )
    logging.info("Version string: %s", ver)

    # Now write the actual header file or put version string in stdout
    if args.header_path == "-":
        print(ver)
    else:
        logging.info("Writing header to %s", args.header_path)
        write_version_header(ver, pathlib.Path(args.header_path), args.static)

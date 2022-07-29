#!/usr/bin/env python3

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to compare Zephyr EC builds"""

import argparse
import atexit
import logging
import pathlib
import shutil
import subprocess
import sys
import tempfile

import zmake.project

original_branch = ""


def get_git_hash(ref):
    """Get the full git commit hash for a git reference

    Args:
        ref: Git reference (e.g. HEAD, m/main, sha256)

    Returns:
        A string, with the full hash of the git reference
    """
    try:
        result = subprocess.run(
            ["git", "rev-parse", ref],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
        )
    except subprocess.CalledProcessedError:
        logging.error("Failed to determine hash for git reference %s", ref)
        sys.exit(1)
    else:
        full_reference = result.stdout.strip()

    return full_reference


def git_current_branch():
    """Returns the name of the current local branch"""
    try:
        result = subprocess.run(
            ["git", "branch", "--show-current"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
        )
    except subprocess.CalledProcessError:
        logging.error("Failed to get the current branch name")
        sys.exit(1)

    return result.stdout.strip()


def restore_branch(branch):
    """Restore to the original branch"""

    logging.info("Restoring local branch to %s", branch)
    try:
        subprocess.run(
            ["git", "checkout", branch],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        logging.error("Failed to restore branch to %s", branch)


def delete_branch(branch):
    """Delete a branch"""

    # Git cannot delete the current branch, so always try to revert
    # back to the user's original branch.
    restore_branch(original_branch)

    logging.info("Deleting temporary branch %s", branch)
    try:
        subprocess.run(
            ["git", "branch", "-D", branch],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        logging.error("Failed to delete branch %s", branch)


def git_create_branch(ref):
    """Create a temporary branch at a specific git reference

    Args:
        ref: Git reference, used to create a temporary branch
    """

    branch = "tmp-cmp-" + ref

    logging.info("    Creating branch %s", branch)

    try:
        subprocess.run(
            ["git", "rev-parse", "--verify", branch],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        # We expect that the branch doesn't exist, so this is the happy path.
        # If the branch exists, stop so we don't overwrite user's data
        logging.info("    Branch doesn't exist, creating it...")
    else:
        logging.error("The local branch %s already exists", branch)
        sys.exit(1)

    # Create the branch
    try:
        subprocess.run(
            ["git", "checkout", "-b", branch, ref],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        logging.error("Failed to create branch %s", branch)
        sys.exit(1)

    logging.info("    Branch is now set to %s", git_current_branch())

    atexit.register(delete_branch, branch)


def run_build(platform_ec_dir, build_dir, project_list, goma):
    """Run the zephyr builds, using build_dir as the output directory"""

    for project in project_list:
        cmd = ["zmake"]
        if goma:
            cmd.append("--goma")
        if project == "testall":
            cmd.append(project)
        else:
            cmd.append("build")
            cmd.append(project)
        cmd.append("--static")
        cmd.append("--clobber")
        cmd.append("-B")
        cmd.append(str(build_dir))

        logging.info("    Building project %s", project)
        try:
            subprocess.run(
                cmd,
                cwd=platform_ec_dir,
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except subprocess.CalledProcessError:
            logging.error("Build for project %s", project)
            sys.exit(1)


def main():
    """Builds multiple revisions fo Zephyr EC targets to copmare binaries"""
    global original_branch

    logging.basicConfig(level=logging.INFO, stream=sys.stderr)

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ref1",
        default="HEAD",
        help="1st git reference (commit, branch, etc)",
    )
    parser.add_argument(
        "--ref2",
        default="HEAD~",
        help="2nd git reference (commit, branch, etc)",
    )
    parser.add_argument(
        "-p",
        "--project",
        action="append",
        help="Specify individual projects to build. If omitted, use testall",
    )
    parser.add_argument(
        "-k",
        "--keep",
        action="store_true",
        help="Keep temporary build directories on exit",
    )
    parser.add_argument(
        "--goma",
        action="store_true",
        help="Use goma system for faster builds (Googlers only)",
    )

    args = parser.parse_args()

    if not args.project:
        args.project = ["testall"]

    original_branch = git_current_branch()
    logging.info("Current branch named %s", original_branch)

    zephyr_dir = pathlib.Path(__file__).parent.resolve()
    platform_ec_dir = zephyr_dir.parent

    git_ref1 = get_git_hash(args.ref1)
    git_ref2 = get_git_hash(args.ref2)

    logging.info("Reference 1 %s", git_ref1)
    logging.info("Reference 2 %s", git_ref2)

    if git_ref1 == git_ref2:
        logging.error("The git hashes match, skipping build comparison")
        sys.exit(1)

    build_parent_dir = tempfile.mkdtemp()
    if not args.keep:
        atexit.register(shutil.rmtree, build_parent_dir)
    else:
        logging.info("Temporary dir %s, will be retained", build_parent_dir)

    build_dir1 = pathlib.Path(build_parent_dir) / git_ref1
    build_dir2 = pathlib.Path(build_parent_dir) / git_ref2

    # Build ref1
    logging.info("Building projects using %s", args.ref1)
    git_create_branch(git_ref1)
    run_build(
        platform_ec_dir=platform_ec_dir,
        build_dir=build_dir1,
        project_list=args.project,
        goma=args.goma,
    )

    # Build ref2
    logging.info("Building projects using %s", args.ref2)
    git_create_branch(git_ref2)
    run_build(
        platform_ec_dir=platform_ec_dir,
        build_dir=build_dir2,
        project_list=args.project,
        goma=args.goma,
    )

    logging.info("Builds complete, starting compare")

    projects_built = []
    if args.project[0] == "testall":
        for project in zmake.project.find_projects(zephyr_dir).values():
            # Only compare output binaries, not the tests
            if project.config.is_test:
                continue
            projects_built.append(project.config.project_name)
    else:
        projects_built = args.project

    failed_projects = []
    for project in projects_built:
        output_bin = pathlib.Path(project) / "output" / "zephyr.bin"
        bin1 = build_dir1 / output_bin
        bin2 = build_dir2 / output_bin
        try:
            subprocess.run(
                ["cmp", bin1, bin2],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except subprocess.CalledProcessError:
            failed_projects.append(project)

    if failed_projects:
        logging.error(
            f"The following projects failed to compare: {failed_projects}"
        )
        sys.exit(1)
    else:
        logging.info("All Zephyr boards compare!")

    return 0


if __name__ == "__main__":
    sys.exit(main())

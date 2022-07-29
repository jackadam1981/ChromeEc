#!/usr/bin/env python3

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to compare Zephyr EC builds"""

import argparse
import atexit
import logging
import os
import os.path
import pathlib
import shutil
import subprocess
import sys
import tempfile

import zmake.modules
import zmake.project
import zmake.util
from zmake.output_packers import packer_registry


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
    except subprocess.CalledProcessError:
        logging.error("Failed to determine hash for git reference %s", ref)
        sys.exit(1)
    else:
        full_reference = result.stdout.strip()

    return full_reference


def run_build(work_dir, project_list, goma):
    """Run the zephyr builds from a temporary work directory

    Args:
        work_dir: Directory containing the zephyr main and all module source.
        project_list: List of Zephyr EC projects to build, or -a to build all
                      projects.
        goma: flag to indicate whether to use goma for the build
    """

    python_path = "PYTHON_PATH=" + str(
        work_dir / "modules" / "ec" / "zephyr" / "zmake"
    )

    for project in project_list:
        cmd = ["env", python_path, "python3", "-m"]
        cmd.append("zmake")
        cmd.append("--modules-dir=" + str(work_dir / "modules"))
        cmd.append("--zephyr-base=" + str(work_dir / "zephyr-base"))
        if goma:
            cmd.append("--goma")
        cmd.extend(["build", project])
        cmd.append("--static")
        cmd.append("--clobber")

        logging.debug("Build cmd: %s", cmd)

        logging.info("    Building project %s", project)
        try:
            subprocess.run(
                cmd,
                cwd=work_dir,
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except subprocess.CalledProcessError:
            logging.error("Build failed for project %s", project)
            sys.exit(1)


def create_bin_from_elf(elf_input, bin_output):
    """Create a plain binary from an ELF executable"""

    cmd = ["objcopy", "-O", "binary"]
    # Some native-posix builds include a GNU build ID, which is guaranteed
    # unique from build to build. Remove this section during conversion
    # binary format.
    cmd.extend(["-R", ".note.gnu.build-id"])
    cmd.extend([elf_input, bin_output])
    try:
        subprocess.run(
            cmd,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        logging.error("Failed to create binary: %s", bin_output)
        sys.exit(1)


def git_do_checkout(module_name, work_dir, git_source, dst_dir, git_ref):
    """Clone a repository and perform a checkout.

    Args:
        module_name: The module name to checkout.
        work_dir: Root directory for the checktout.
        git_source: Path to the repository for the module.
        dst_dir: Destination directory for the checkout, relative to the work_dir.
        git_ref: Git reference to checkout.
    """
    cmd = ["git", "clone", "--quiet", "--no-checkout", git_source, dst_dir]

    try:
        subprocess.run(
            cmd,
            cwd=work_dir,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        logging.error("Clone failed for %s", module_name)
        sys.exit(1)

    cmd = ["git", "-C", dst_dir, "checkout", "--quiet", git_ref]
    try:
        subprocess.run(
            cmd,
            cwd=work_dir,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        logging.error("Checkout of %s failed for %s", git_ref, module_name)
        sys.exit(1)


def checkout_source(zephyr_base, module_paths, ec_ref, work_dir):
    """Create the Zephyr EC sources from all required modules

    Args:
        zephyr_base: Full path to the Zephyr source
        module_paths: Dictionary of module name/source path tuples.
        ec_ref: Git reference to use for the EC module only.  The Zephyr source
                and other moduldes always use HEAD.
        work_dir: Root directory where all sources are checked out.
    """

    logging.info("Checkig out all EC sources for ref %s", ec_ref)

    for module_name, git_source in module_paths.items():
        dst_dir = pathlib.Path("modules/") / module_name
        git_ref = ec_ref if module_name == "ec" else "HEAD"
        git_do_checkout(
            module_name=module_name,
            work_dir=work_dir,
            git_source=git_source,
            dst_dir=dst_dir,
            git_ref=git_ref,
        )

    git_do_checkout(
        module_name="zephyr",
        work_dir=work_dir,
        git_source=zephyr_base,
        dst_dir="zephyr-base",
        git_ref="HEAD",
    )


def main():
    """Builds multiple revisions fo Zephyr EC targets to compare binaries"""

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
        help="Specify individual projects to build. If omitted, build all projects",
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

    # If no projects specified, build them all
    if not args.project:
        args.project = ["-a"]

    checkout = zmake.util.locate_cros_checkout()
    module_paths = zmake.modules.locate_from_checkout(checkout)
    zephyr_base = checkout / "src" / "third_party" / "zephyr" / "main"

    ec_zephyr_dir = pathlib.Path(__file__).parent.resolve()

    logging.debug("Checkout %s", checkout)
    logging.debug("Zephyr base %s", zephyr_base)
    logging.debug("Module paths %s", module_paths)

    git_ref1 = get_git_hash(args.ref1)
    git_ref2 = get_git_hash(args.ref2)

    logging.debug("EC Git Reference 1 %s", git_ref1)
    logging.debug("EC Git Reference 2 %s", git_ref2)

    if git_ref1 == git_ref2:
        logging.error("The git hashes match, skipping build comparison")
        sys.exit(1)

    build_parent_dir = tempfile.mkdtemp(prefix="zcompare-")
    if not args.keep:
        atexit.register(shutil.rmtree, build_parent_dir)
    else:
        logging.info("Temporary dir %s, will be retained", build_parent_dir)

    build_dir1 = pathlib.Path(build_parent_dir) / git_ref1
    os.mkdir(build_dir1)

    build_dir2 = pathlib.Path(build_parent_dir) / git_ref2
    os.mkdir(build_dir2)

    # Copy sources to the build directory
    checkout_source(
        zephyr_base=zephyr_base,
        module_paths=module_paths,
        ec_ref=git_ref1,
        work_dir=build_dir1,
    )

    checkout_source(
        zephyr_base=zephyr_base,
        module_paths=module_paths,
        ec_ref=git_ref2,
        work_dir=build_dir2,
    )

    logging.info("Building projects using %s (%s)", args.ref1, git_ref1)
    run_build(work_dir=build_dir1, project_list=args.project, goma=args.goma)

    logging.info("Building projects using %s (%s)", args.ref2, git_ref2)
    run_build(work_dir=build_dir2, project_list=args.project, goma=args.goma)

    logging.info("Builds complete, starting compare")

    projects_built = set()
    all_projects = zmake.project.find_projects(ec_zephyr_dir)
    if args.project[0] == "-a":
        projects_built = {
            project
            for project in all_projects.values()
            if not project.config.is_test
        }
    else:
        for project_name in args.project:
            try:
                projects_built.add(all_projects[project_name])
            except KeyError as error:
                raise KeyError(
                    "No project named {}".format(project_name)
                ) from error

    failed_projects = []
    missing_projects = []
    for project in projects_built:
        output_path = (
            pathlib.Path("modules")
            / "ec"
            / "build"
            / "zephyr"
            / pathlib.Path(project.config.project_name)
            / "output"
        )
        output_dir1 = build_dir1 / output_path
        output_dir2 = build_dir2 / output_path

        bin_output1 = output_dir1 / "ec.bin"
        bin_output2 = output_dir2 / "ec.bin"

        # ELF executables don't compare due to meta data.  Convert to a binary
        # for the comparison
        if project.config.output_packer == packer_registry["elf"]:
            create_bin_from_elf(
                elf_input=output_dir1 / "zephyr.elf", bin_output=bin_output1
            )
            create_bin_from_elf(
                elf_input=output_dir2 / "zephyr.elf", bin_output=bin_output2
            )

        bin1_path = pathlib.Path(bin_output1)
        bin2_path = pathlib.Path(bin_output2)
        if not os.path.isfile(bin1_path) or not os.path.isfile(bin2_path):
            logging.error("Output binary %s not found", output_path)
            missing_projects.append(project)
            continue

        try:
            subprocess.run(
                ["cmp", bin_output1, bin_output2],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except subprocess.CalledProcessError:
            failed_projects.append(project.config.project_name)

    if failed_projects or missing_projects:
        logging.error(
            "The following projects failed to compare: %s", failed_projects
        )
        logging.error(
            "The following projects are missing ec.bin: %s", missing_projects
        )
        sys.exit(1)
    else:
        logging.info("All Zephyr boards compare!")

    return 0


if __name__ == "__main__":
    sys.exit(main())

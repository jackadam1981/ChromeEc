# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for RWSIG signing. """

import logging
import os.path
from pathlib import Path
import subprocess
from typing import List, Union

from zmake import util
import zmake.jobserver
import zmake.multiproc


def sign_fw(
    bin_file: Path,
    key_path: Path,
    work_dir: Path,
    jobclient: zmake.jobserver.JobClient,
    logger: logging.Logger,
) -> Path:
    """Sign a firmware image using rwsig algorithm.

    Args:
        bin_file: Path to the unsigned EC binary.
        work_dir: A directory to write outputs and temporary files into.
        jobclient: A JobClient object to use.
        logger: The Logger object to log to.
        key_path: Path to the PEM encoded key.

    Returns:
        Path to the signed firmware.
    """
    ec_rw = work_dir / "ec_rw"
    pub_key = work_dir / "key.vbpubk2"
    pri_key = work_dir / "key.vbprik2"
    sig_file = work_dir / "ec.sig"
    signed_bin = work_dir / "ec-signed.bin"

    _run_futility(
        ["dump_fmap", "-x", bin_file, f"EC_RW:{ec_rw}", f"SIG_RW:{sig_file}"],
        work_dir,
        jobclient,
        logger,
    )
    data_size = os.path.getsize(ec_rw) - os.path.getsize(sig_file)

    _run_futility(
        ["create", key_path, work_dir / "key"], work_dir, jobclient, logger
    )

    _run_futility(
        [
            "sign",
            "--type",
            "rwsig",
            "--data_size",
            str(data_size),
            "--prikey",
            pri_key,
            ec_rw,
            sig_file,
        ],
        work_dir,
        jobclient,
        logger,
    )

    _run_futility(
        [
            "load_fmap",
            "-o",
            signed_bin,
            bin_file,
            f"KEY_RO:{pub_key}",
            f"SIG_RW:{sig_file}",
        ],
        work_dir,
        jobclient,
        logger,
    )

    return signed_bin


def _run_futility(
    args: List[Union[str, Path]],
    work_dir: Path,
    jobclient: zmake.jobserver.JobClient,
    logger: logging.Logger,
) -> None:
    """Helper to execute futility command.

    Args:
        args:
        work_dir: A directory to write outputs and temporary files into.
        jobclient: A JobClient object to use.
        logger: The Logger object to log to.
    """
    proc = jobclient.popen(
        [util.get_tool_path("futility"), *args],
        cwd=work_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
    )
    zmake.multiproc.LogWriter.log_output(logger, logging.DEBUG, proc.stdout)
    zmake.multiproc.LogWriter.log_output(logger, logging.ERROR, proc.stderr)
    proc.wait(timeout=60)
    if proc.returncode:
        raise subprocess.CalledProcessError(proc.returncode, proc.args)

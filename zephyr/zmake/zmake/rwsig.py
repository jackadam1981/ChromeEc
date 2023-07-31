# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes """
import logging
import os.path
from pathlib import Path
import subprocess
from typing import Dict

import zmake.build_config as build_config
import zmake.jobserver
import zmake.util as util


def rwsig_sign(
        bin_file: Path,
        work_dir: Path,
        jobclient: zmake.jobserver.JobClient,
        logger: logging.Logger,
        dir_map: Dict[str, Path],
        ec_dir: Path):
    """Sign a firmware image using rwsig algorithm.

    Args:
        bin_file: Path to the unsigned EC binary.
        work_dir: A directory to write outputs and temporary files into.
        jobclient: A JobClient object to use.
        logger: The Logger object to log to.
        dir_map: A dict of build dirs such as {'ro': path_to_ro_dir}.
        ec_dir: Path to EC module.

    Returns:
        A 2-tuple: (path to signed rw, signature blob)
    """
    ec_rw = work_dir / "ec_rw"
    pub_key = work_dir / "key.vbpubk2"
    pri_key = work_dir / "key.vbprik2"
    sig_file = work_dir / "ec.sig"
    signed_bin = work_dir / "ec-signed.bin"

    key_path = util.read_kconfig_autoconf_value(
        dir_map["rw"] / "zephyr" / "include" / "generated",
        "CONFIG_PLATFORM_EC_RWSIG_KEY_PATH",
    )[1:-1]

    _run_futility(["dump_fmap",
                   "-x",
                   bin_file,
                   f"EC_RW:{ec_rw}",
                   f"SIG_RW:{sig_file}"],
                  work_dir, jobclient, logger)
    data_size = os.path.getsize(ec_rw) - os.path.getsize(sig_file)

    _run_futility(["create",
                   ec_dir / key_path,
                   work_dir / "key"],
                  work_dir, jobclient, logger)

    _run_futility(["sign",
                   "--type", "rwsig",
                   "--data_size", str(data_size),
                   "--prikey", pri_key,
                   ec_rw,
                   sig_file],
                  work_dir, jobclient, logger)

    _run_futility(["load_fmap", "-o", signed_bin,
                   bin_file,
                   f"KEY_RO:{pub_key}",
                   f"SIG_RW:{sig_file}"],
                  work_dir, jobclient, logger)

    return signed_bin, sig_file

def _run_futility(args, work_dir, jobclient, logger):
    proc = jobclient.popen(
        [util.get_tool_path("futility"), *args],
        cwd=work_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
    )
    zmake.multiproc.LogWriter.log_output(
        logger, logging.DEBUG, proc.stdout
    )
    zmake.multiproc.LogWriter.log_output(
        logger, logging.ERROR, proc.stderr
    )
    proc.wait(timeout=60)
    if proc.returncode:
        raise subprocess.CalledProcessError(
                f"Failed to execute futility {args[0]}")

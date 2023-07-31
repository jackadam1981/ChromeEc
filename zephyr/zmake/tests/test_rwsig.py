# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test rwsig signing."""

import logging
import pathlib
import subprocess
import tempfile
from unittest import mock

from zmake import jobserver
from zmake import multiproc
from zmake import rwsig


FILES_DIR = pathlib.Path(__file__).parent / "files"


def test_sign_firmware():
    """Test the signing function."""
    jobclient = jobserver.JobClient()
    logger = mock.Mock(spec=logging.Logger)
    multiproc.LogWriter.reset()

    dts_path = FILES_DIR / "rwsig.dts"

    with tempfile.TemporaryDirectory() as temp_dir_name:
        temp_dir = pathlib.Path(temp_dir_name)

        # Create a fake EC image and a RSA key.
        subprocess.run(
            ["binman", "build", "--dt", dts_path, "--outdir", temp_dir],
            check=True,
        )
        key_path = temp_dir / "key.pem"
        subprocess.run(
            ["openssl", "genrsa", "-3", "-out", key_path, "3072"], check=True
        )

        # Sign the firmware.
        signed_fw = rwsig.sign_fw(
            temp_dir / "ec.bin", key_path, temp_dir, jobclient, logger
        )

        # Verify signature.
        subprocess.run(["futility", "verify", signed_fw], check=True)

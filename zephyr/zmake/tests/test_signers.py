# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for signers."""

import os.path
import pathlib
from unittest import mock

from zmake import signers


def fake_getsize(path):
    """Mocked os.path.getsize that returns fake size for EC_RW and SIG_RW."""

    if path.name == "ec_rw":
        return 10000
    if path.name == "ec.sig":
        return 100

    raise ValueError


@mock.patch.object(os.path, "getsize", side_effect=fake_getsize)
@mock.patch.object(signers.RwsigSigner, "_run_futility")
def test_rwsig_sign(mock_futility, mock_getsize):
    """Test rwsig signing.

    We can't call futility here, so this test only verifies the
    number of bytes to sign is good.
    """
    del mock_getsize

    signer = signers.RwsigSigner(pathlib.Path())

    signer.sign(pathlib.Path(), pathlib.Path(), None)

    mock_futility.assert_any_call(
        ["sign", "--type", "rwsig", "--data_size", "9900"] + [mock.ANY] * 4,
        mock.ANY,
        mock.ANY,
    )

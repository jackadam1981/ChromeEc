# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test the zmake re-exec functionality."""

import os
import sys
import unittest.mock as mock

import pytest

import zmake.__main__ as main
from tests.common import create_fake_checkout


@pytest.fixture
def fake_env(monkeypatch):
    environ = {}
    monkeypatch.setattr(os, "environ", environ)
    return environ


@pytest.fixture
def mock_execve():
    with mock.patch("os.execve", autospec=True) as mocked_function:
        yield mocked_function


def test_out_of_chroot(fake_env, mock_execve):
    # When CROS_WORKON_SRCROOT is not set, we should not re-exec.
    main.maybe_reexec(["--help"])
    mock_execve.assert_not_called()


def test_pythonpath_set(fake_env, mock_execve):
    # With PYTHONPATH set, we should not re-exec.
    fake_env["CROS_WORKON_SRCROOT"] = "/mnt/host/source"
    fake_env["PYTHONPATH"] = "/foo/bar/baz"
    main.maybe_reexec(["--help"])
    mock_execve.assert_not_called()


def test_zmake_does_not_exist(fake_env, mock_execve):
    # When zmake is not at src/platform/ec/zephyr/zmake, don't re-exec.
    fake_env["CROS_WORKON_SRCROOT"] = "/this/does/not/exist"
    main.maybe_reexec(["--help"])
    mock_execve.assert_not_called()


def test_zmake_reexec(fake_env, mock_execve):
    # Nothing else applies?  The re-exec should happen.
    actual_zmake_src_path = pathlib.Path(__file__).parent.parent
    # Create a fake chroot filesystem
    with create_fake_checkout() as (fake_srcroot, fake_zmake_path):
        fake_env["CROS_WORKON_SRCROOT"] = fake_srcroot
        main.maybe_reexec(["--help"])
        new_env = dict(fake_env)
        new_env["PYTHONPATH"] = str(fake_zmake_path.resolve())
        mock_execve.assert_called_once_with(
            sys.executable,
            [sys.executable, "-m", "zmake", "--help"],
            new_env,
        )

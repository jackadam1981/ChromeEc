# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helpful functions for unit tests."""

import os
import pathlib
import tempfile
from contextlib import contextmanager

import zmake.zmake as zm


@contextmanager
def zmake_from_dir(**kwds):
    """Creates module dirs and returns a Zmake object."""

    with tempfile.TemporaryDirectory() as tmpname:
        tmpname = pathlib.Path(tmpname)
        os.mkdir(tmpname / "ec")
        os.mkdir(tmpname / "ec" / "zephyr")
        (tmpname / "ec" / "zephyr" / "module.yml").write_text("")
        zephyr_base = tmpname / "zephyr_base"
        yield zm.Zmake(zephyr_base=zephyr_base, modules_dir=tmpname, **kwds)


@contextmanager
def create_fake_checkout():
    """Creates a fake checkout dir and returns the path."""

    actual_zmake_src_path = pathlib.Path(__file__).parent.parent
    with tempfile.TemporaryDirectory() as fake_srcroot:
        fake_zmake_path = pathlib.Path(fake_srcroot) / "src/platform/ec/zephyr/zmake"
        os.makedirs(fake_zmake_path.parent)
        os.symlink(actual_zmake_src_path, fake_zmake_path)
        yield (fake_srcroot, fake_zmake_path)


@contextmanager
def zmake_from_checkout(**kwds):
    """Creates a fake checkout dir and returns a Zmake object."""

    with create_fake_checkout() as (fake_srcroot, _):
        yield zm.Zmake(checkout=fake_srcroot, **kwds)

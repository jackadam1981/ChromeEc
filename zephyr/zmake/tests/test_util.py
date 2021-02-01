# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib

import zmake.util as util


def test_resolve_build_dir_with_build_dir():
    build_dir = util.resolve_build_dir(
        platform_ec_dir=pathlib.PosixPath('/x'),
        project_dir=pathlib.PosixPath('/x/y'),
        build_dir=pathlib.PosixPath('/x/z'))

    assert build_dir == pathlib.PosixPath('/x/z')


def test_resolve_build_dir_default_dir():
    build_dir = util.resolve_build_dir(
        platform_ec_dir=pathlib.PosixPath('/x'),
        project_dir=pathlib.PosixPath('/x/y/z'),
        build_dir=None)
    assert build_dir == pathlib.PosixPath('/x/build/y/z')

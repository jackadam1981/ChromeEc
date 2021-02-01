# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import pytest
import tempfile

import zmake.util as util


def test_resolve_build_dir_with_build_dir():
    with tempfile.TemporaryDirectory() as temp_dir_name:
        temp_dir = pathlib.Path(temp_dir_name)
        (temp_dir / 'x' / 'z' / 'project').mkdir(parents=True)
        (temp_dir / 'x' / 'z' / 'project' / 'zmake.yaml').touch()
        build_dir = util.resolve_build_dir(
            platform_ec_dir=temp_dir / 'x',
            project_dir=temp_dir / 'x' / 'y',
            build_dir=temp_dir / 'x' / 'z')

        assert build_dir == temp_dir / 'x' / 'z'


def test_resolve_build_dir_invalid_project():
    try:
        with tempfile.TemporaryDirectory() as temp_dir_name:
            temp_dir = pathlib.Path(temp_dir_name)
            util.resolve_build_dir(
                platform_ec_dir=temp_dir / 'x',
                project_dir=temp_dir / 'x' / 'y' / 'z',
                build_dir=None)
            pytest.fail()
    except Exception:
        pass


def test_resolve_build_dir_from_project():
    """Since the build dir is not a configured build directory but instead a
    project directory, it should be ignored.
    """
    with tempfile.TemporaryDirectory() as temp_dir_name:
        temp_dir = pathlib.Path(temp_dir_name)
        project_dir = temp_dir / 'x' / 'y' / 'z'
        project_dir.mkdir(parents=True)
        (project_dir / 'zmake.yaml').touch()
        # Test when project_dir == build_dir.
        build_dir = util.resolve_build_dir(
            platform_ec_dir=temp_dir / 'x',
            project_dir=project_dir,
            build_dir=project_dir)
        assert build_dir == temp_dir / 'x' / 'build' / 'y' / 'z'

        # Test when build_dir is None (it should be ignored).
        build_dir = util.resolve_build_dir(
            platform_ec_dir=temp_dir / 'x',
            project_dir=project_dir,
            build_dir=None)
        assert build_dir == temp_dir / 'x' / 'build' / 'y' / 'z'

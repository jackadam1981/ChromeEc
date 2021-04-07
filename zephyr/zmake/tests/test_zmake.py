# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Do a run of 'zmake build' and check the output"""

import logging
import os
import pathlib
import re
import tempfile
import unittest.mock as mock
from unittest.mock import patch

import zmake.jobserver
import zmake.project
import zmake.zmake as zm
from testfixtures import LogCapture

OUR_PATH = os.path.dirname(os.path.realpath(__file__))


class FakeProject:
    """A fake project which requests two builds and does no packing"""
    # pylint: disable=too-few-public-methods

    def __init__(self):
        self.packer = mock.Mock()
        self.packer.pack_firmware = mock.Mock(return_value=[])
        self.project_dir = pathlib.Path('FakeProjectDir')

    @staticmethod
    def iter_builds():
        """Yield the two builds that zmake normally does"""
        yield 'build-ro', None
        yield 'build-rw', None


class FakeJobserver(zmake.jobserver.GNUMakeJobServer):
    """A fake jobserver which just runs 'cat' on the provided files"""

    def __init__(self, fnames):
        """Start up a jobserver with two jobs

        Args:
            fnames: Dict of regexp to filename. If the regexp matches the
            command, then the filename will be returned as the output.
        """
        super().__init__()
        self.jobserver = zmake.jobserver.GNUMakeJobServer(jobs=2)
        self.fnames = fnames

    def get_job(self):
        """Fake implementation of get_job(), which returns a real JobHandle()"""
        return zmake.jobserver.JobHandle(mock.Mock())

    # pylint: disable=arguments-differ
    def popen(self, cmd, *args, **kwargs):
        """Ignores the provided command and just runs 'cat' instead"""
        for pattern, filename in self.fnames.items():
            if pattern.match(" ".join(cmd)):
                new_cmd = ['cat', filename]
                break
        else:
            raise Exception('No pattern matched "%s"' % " ".join(cmd))
        return self.jobserver.popen(new_cmd, *args, **kwargs)


def do_test_with_log_level(log_level):
    """Test filtering using a particular log level

    Args:
        log_level: Level to use

    Returns:
        tuple:
            - List of log strings obtained from the run
            - Temporary directory used for build
    """
    fnames = {
        re.compile(r".*build-ro"): os.path.join(
            OUR_PATH, 'files', 'sample_ro.txt'),
        re.compile(r".*build-rw"): os.path.join(
            OUR_PATH, 'files', 'sample_rw.txt'),
    }
    zmk = zm.Zmake(jobserver=FakeJobserver(fnames))

    with LogCapture(level=log_level) as cap:
        with tempfile.TemporaryDirectory() as tmpname:
            with patch('zmake.version.get_version_string', return_value='123'):
                with patch.object(zmake.project, 'Project',
                                  return_value=FakeProject()):
                    zmk.build(pathlib.Path(tmpname))
    recs = [rec.getMessage() for rec in cap.records]
    return recs, tmpname


def test_filter_normal():
    """Test filtering of a normal build (with no errors)"""
    recs, _ = do_test_with_log_level(logging.ERROR)
    assert not recs


def test_filter_info():
    """Test what appears on the INFO level"""
    recs, tmpname = do_test_with_log_level(logging.INFO)
    # This produces an easy-to-read diff if there is a difference
    assert set(recs) == {
        'Building %s:build-ro: /usr/bin/ninja -C %s/build-build-ro' %
        (tmpname, tmpname),
        'Building %s:build-rw: /usr/bin/ninja -C %s/build-build-rw' %
        (tmpname, tmpname),
        '[%s:build-ro]FLASH:      241868 B       512 KB     46.13%%' % tmpname,
        '[%s:build-ro]IDT_LIST:          0 GB         2 KB      0.00%%' % tmpname,
        '[%s:build-ro]Memory region         Used Size  Region Size  %%age Used' % tmpname,
        '[%s:build-ro]SRAM:       48632 B        62 KB     76.60%%' % tmpname,
        '[%s:build-rw]FLASH:      241868 B       512 KB     46.13%%' % tmpname,
        '[%s:build-rw]IDT_LIST:          0 GB         2 KB      0.00%%' % tmpname,
        '[%s:build-rw]Memory region         Used Size  Region Size  %%age Used' % tmpname,
        '[%s:build-rw]SRAM:       48632 B        62 KB     76.60%%' % tmpname,
    }

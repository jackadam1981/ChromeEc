# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for powerlog."""

import os
import shutil
import tempfile
import unittest

import powerlog

class TestPowerlog(unittest.TestCase):
  """Test to verify powerlog util methods work as expected."""

  def setUp(self):
    """Set up data and create a temporary directory to save data and stats."""
    self.tempdir = tempfile.mkdtemp()
    self.filename = 'testfile'
    self.filepath = os.path.join(self.tempdir, self.filename)
    with open(self.filepath, 'w') as f:
      f.write('')

  def tearDown(self):
    """Delete the temporary directory and its content."""
    shutil.rmtree(self.tempdir)

  def test_ProcessFilenameAbsoluteFilePath(self):
    """Absolute file path is returned unchanged."""
    processed_fn = powerlog_util.process_filename(self.filepath)
    self.assertEqual(self.filepath, processed_fn)

  def test_ProcessFilenameRelativeFilePathCWD(self):
    """Correct path is returned when using relative name inside cwd."""
    original_wd = os.getcwd()
    os.chdir(self.tempdir)
    processed_fn = powerlog_util.process_filename(self.filename)
    try:
      self.assertEqual(self.filepath, processed_fn)
    finally:
      os.chdir(original_wd)

  def test_ProcessFilenameLibDir(self):
    """Correct path is returned when using relative name inside LIBDIR."""
    original_lib_dir = powerlog_util.LIB_DIR
    powerlog_util.LIB_DIR = self.tempdir
    processed_fn = powerlog_util.process_filename(self.filename)
    try:
      self.assertEqual(self.filepath, processed_fn)
    finally:
      powerlog_util.LIB_DIR = original_lib_dir

  def test_ProcessFilenameRelativeFilePathPyFile(self):
    """Correct path is returned when filename is in same dir as powerlog_util"""
    original__file__ = powerlog_util.__file__
    powerlog_util.__file__ = os.path.join(self.tempdir,
                                          os.path.basename(original__file__))
    processed_fn = powerlog_util.process_filename(self.filename)
    try:
      self.assertEqual(self.filepath, processed_fn)
    finally:
      powerlog_util.__file__ = original__file__

  def test_ProcessFilenameInvalid(self):
    """IOError is raised when file cannot be found by any of the four ways."""
    with self.assertRaises(IOError):
      powerlog_util.process_filename(self.filename)

if __name__ == '__main__':
  unittest.main()

#!/usr/bin/env python3
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test fptool.py."""

import pathlib
import tempfile
import unittest
from unittest.mock import patch

import fptool


class TestUtils(unittest.TestCase):
    def test_util_exists(self):
        self.assertTrue(fptool.util_exists(pathlib.Path("ls")))
        self.assertFalse(fptool.util_exists(pathlib.Path("not_a_real_cmd")))

    def test_run(self):
        self.assertEqual(fptool.run(["true"]), 0)
        self.assertEqual(fptool.run(["false"]), 1)


class TestMain(unittest.TestCase):
    @patch("fptool.util_exists", return_value=True)
    @patch("fptool.run", return_value=0)
    def test_flash_without_image(self, mock_run, mock_util_exists):
        """Ensure that we forward the no firmware image case through to
        flash_fp_mcu.
        """
        self.assertEqual(fptool.main(["flash"]), 0)
        mock_run.assert_called_once_with(["flash_fp_mcu"])

    @patch("fptool.util_exists", return_value=True)
    @patch("fptool.run", return_value=0)
    def test_flash_with_image(self, mock_run, mock_util_exists):
        """Ensure that we forward the firmware image path to flash_fp_mcu."""
        with tempfile.NamedTemporaryFile() as temp_file:
            fptool.main(["flash", temp_file.name])
            mock_run.assert_called_once_with(["flash_fp_mcu", temp_file.name])


if __name__ == "__main__":
    unittest.main()

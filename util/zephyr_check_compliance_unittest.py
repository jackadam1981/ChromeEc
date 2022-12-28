#!/usr/bin/env vpython3

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for check_zephyr_project_config.py"""

import sys
import unittest

import mock  # pylint:disable=import-error
import zephyr_check_compliance

# pylint:disable=protected-access


class TestZephyrCheckCompliance(unittest.TestCase):
    """Tests for zephyr_check_compliance."""

    @mock.patch("check_compliance.get_files")
    def test_changed_files_prefix(self, get_files_mock):
        """Test _changed_files_prefix."""
        get_files_mock.return_value = [
            "a/file",
            "b/file",
            "c/file",
        ]

        out = zephyr_check_compliance._changed_files_prefix("x/", "ref")
        self.assertFalse(out)
        out = zephyr_check_compliance._changed_files_prefix("b/", "ref")
        self.assertTrue(out)

    @mock.patch("zephyr_check_compliance._changed_files_prefix")
    @mock.patch("check_compliance.main")
    def test_main(self, main_mock, changed_files_prefix_mock):
        """Tests the main function."""
        changed_files_prefix_mock.return_value = True
        sys.argv = [""]

        zephyr_check_compliance.main(["ref"])

        changed_files_prefix_mock.assert_called_with("zephyr/", "ref~1..ref")
        main_mock.assert_called_with()
        self.assertEqual(
            sys.argv,
            [
                "<internal>",
                "-m",
                "YAMLLint",
                "-m",
                "DevicetreeBindings",
                "-c",
                "ref~1..ref",
            ],
        )

    @mock.patch("zephyr_check_compliance._changed_files_prefix")
    @mock.patch("check_compliance.main")
    def test_main_skip(self, main_mock, changed_files_prefix_mock):
        """Tests the main function."""
        changed_files_prefix_mock.return_value = False

        zephyr_check_compliance.main(["ref"])

        changed_files_prefix_mock.assert_called_with("zephyr/", "ref~1..ref")
        self.assertEqual(main_mock.call_count, 0)


if __name__ == "__main__":
    unittest.main()

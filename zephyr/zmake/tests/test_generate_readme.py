# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the generate_readme.py file."""

# pylint:disable=import-error
import argparse

import pytest
import zmake.generate_readme as gen_readme


def test_generate_readme_contents():
    """Verify the readme contains at least one known line."""
    readme = gen_readme.generate_readme()

    # Look for a string we know should appear in the README.
    assert "### zmake build\n" in readme


def test_generate_readme_help_format_expansion():
    """Verify that MarkdownHelpFormatter correctly formats help text"""
    test_parser = argparse.ArgumentParser(prog="TestProg")
    test_parser.add_argument("arg1", help="This should be one percent sign: %%")
    test_parser.add_argument("arg2", help="This is the program name: %(prog)s")
    test_parser.formatter_class = gen_readme.MarkdownHelpFormatter

    assert "This should be one percent sign: %" in test_parser.format_help()
    assert "This is the program name: TestProg" in test_parser.format_help()


@pytest.mark.parametrize(
    ["expected_contents", "actual_contents", "return_code"],
    [
        ("abc\ndef\nghi\n", "abc\nghi\n", 1),
        ("abc\ndef\nghi\n", "abc\ndef\nghi\n", 0),
        ("abc\ndef\nghi\n", None, 1),
    ],
)
def test_generate_readme_diff(
    monkeypatch,
    tmp_path,
    zmake_from_dir,
    expected_contents,
    actual_contents,
    return_code,
):
    """Verify that the diff function can detect different text."""

    def generate_readme():
        return expected_contents

    monkeypatch.setattr(gen_readme, "generate_readme", generate_readme)

    readme_file = tmp_path / "README.md"
    if actual_contents is not None:
        readme_file.write_text(actual_contents)

    assert zmake_from_dir.generate_readme(readme_file, diff=True) == return_code


@pytest.mark.parametrize("exist", [False, True])
def test_generate_readme_file(monkeypatch, tmp_path, exist, zmake_from_dir):
    """Verify that README.md can be created if it exists already or not."""

    def generate_readme():
        return "hello\n"

    monkeypatch.setattr(gen_readme, "generate_readme", generate_readme)

    readme_file = tmp_path / "README.md"
    if exist:
        readme_file.write_text("some existing contents\n")

    assert zmake_from_dir.generate_readme(readme_file) == 0
    assert readme_file.read_text() == "hello\n"

# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hypothesis
import hypothesis.strategies as st
import pathlib
import string
import tempfile

import zmake.modules


board_names = st.text(alphabet=set(string.printable) - {'/', ';'}, min_size=1)
sets_of_board_names = st.lists(st.lists(board_names))


@hypothesis.given(sets_of_board_names)
def test_find_dts_overlays(modules):
    """Test the functionality of find_dts_overlays with multiple
    modules, each with sets of board names."""

    # Recursive function to wind up all the temporary directories and
    # call the actual test.
    def setup_modules_and_dispatch(modules, test_fn, module_list=()):
        if modules:
            boards = modules[0]
            with tempfile.TemporaryDirectory() as modpath:
                modpath = pathlib.Path(modpath)
                for board in boards:
                    dts_path = zmake.modules.dts_overlay_name(modpath, board)
                    dts_path.parent.mkdir(parents=True, exist_ok=True)
                    dts_path.touch()
                setup_modules_and_dispatch(
                    modules[1:], test_fn, module_list=module_list + (modpath,))
        else:
            test_fn(module_list)

    # The actual test case, once temp modules have been setup.
    def testcase(module_paths):
        # Maps board_name→overlay_files
        board_file_mapping = {}
        for modpath, board_list in zip(module_paths, modules):
            for board in board_list:
                file_name = zmake.modules.dts_overlay_name(modpath, board)
                files = board_file_mapping.get(board, set())
                board_file_mapping[board] = files | {file_name}

        for board, expected_dts_files in board_file_mapping.items():
            config = zmake.modules.find_dts_overlays(
                board, dict(enumerate(module_paths)))

            assert (set(config.cmake_defs.get('DTC_OVERLAY_FILE', '').split(';'))
                    == set(map(str, expected_dts_files)))

    setup_modules_and_dispatch(modules, testcase)

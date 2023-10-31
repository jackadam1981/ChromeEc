# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"Common functions for other scripts"

import inspect
import logging
import pickle
import re
import site


def _load_edt(zephyr_base, edt_pickle):
    """Load an EDT object from a pickle file source.

    Args:
        zephyr_base: pathlib.Path pointing to the Zephyr OS repository.
        edt_pickle: pathlib.Path pointing to the EDT object, stored as a pickle
            file.

    Returns:
        A 3-field tuple: (edtlib, edt, project_name)
            edtlib: module object for the edtlib
            edt: EDT object of the devicetree
            project_name: string containing the name of the project or test.

        Returns None if the edtlib pickle file doesn't exist.
    """
    zephyr_devicetree_path = (
        zephyr_base / "scripts" / "dts" / "python-devicetree" / "src"
    )

    # Add Zephyr's python-devicetree into the source path.
    site.addsitedir(zephyr_devicetree_path)

    try:
        with open(edt_pickle, "rb") as edt_file:
            edt = pickle.load(edt_file)
    except FileNotFoundError:
        # Skip the all EC specific checks if the edt_pickle file doesn't exist.
        # UnpicklingErrors will raise an exception and fail the build.
        return None, None, None

    is_test = re.compile(r"twister-out")

    if is_test.search(edt_pickle.as_posix()):
        # For tests built with twister, the edt.pickle file is located in a
        # path ending <test_name>/zephyr/.
        project_name = edt_pickle.parents[1].name
    else:
        # For Zephyr EC project, the edt.pickle file is located in a path
        # ending <project>/build-[ro|rw|single-image]/zephyr/.
        project_name = edt_pickle.parents[2].name

    edtlib = inspect.getmodule(edt)

    return edtlib, edt, project_name


# Dictionary used to map log level strings to their corresponding int values.
log_level_map = {
    "DEBUG": logging.DEBUG,
    "INFO": logging.INFO,
    "WARNING": logging.WARNING,
    "ERROR": logging.ERROR,
    "CRITICAL": logging.CRITICAL,
}

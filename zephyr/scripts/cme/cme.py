# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Component Manifest Engine"""

import argparse
import inspect
import logging
from pathlib import Path
import pickle
import re
import site
import sys
from typing import List, Optional


g_manifest = ""
i2c_portmap = dict()
first_component = True


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

def parse_args(argv: Optional[List[str]] = None):
    """Returns parsed command-line arguments"""
    parser = argparse.ArgumentParser(
        prog="named_gpios",
        description="Zephyr EC specific devicetree checks",
    )

    parser.add_argument(
        "--zephyr-base",
        type=Path,
        help="Path to Zephyr OS repository",
        required=True,
    )

    parser.add_argument(
        "--edt-pickle",
        type=Path,
        help="EDT object file, in pickle format",
        required=True,
    )

    parser.add_argument(
        "--manifest-file",
        type=Path,
        help="Path to the component manifest JSON file in JSON format",
        required=True,
    )

    parser.add_argument(
        "-l",
        "--log-level",
        choices=log_level_map.values(),
        metavar=f"{{{','.join(log_level_map)}}}",
        type=lambda x: log_level_map[x],
        default=logging.INFO,
        help="Set the logging level (default=INFO)",
    )

    return parser.parse_args(argv)


def iterate_i2c_ports(edtlib, edt):
    """EDIT IT: Verify there are no duplicate GPIOs in the named-gpios node.

    Args:
        edtlib: Module object for the edtlib library.
        edt: EDT object representation of a devicetree

    Returns:
        True if no duplicates found.  Returns False otherwise.
    """
    # Dictionary of GPIO controllers, indexed by the GPIO controller nodelabel
    try:
        named_i2c_ports = edt.get_node("/named-i2c-ports")
    except edtlib.EDTError:
        # If the usbc node doesn't exist, return success.
        logging.error("Not exist i2c")
        return True

    i = 0
    for node in named_i2c_ports.children.values():
        if len(node.labels):
            i2c_label = node.props['i2c-port'].val.label
            if 'remote-port' in node.props:
                i2c_portmap[i2c_label] = node.props['remote-port'].val
            else:
                i2c_portmap[i2c_label] = i
        i = i + 1

    return True


def output_str(s):
    """Output a string to the component manifest."""
    global g_manifest
    g_manifest += s


def output_line(s):
    """Output a line to the component manifest."""
    output_str(s + '\n')


def output_header():
    """Output the component manifest header."""
    output_line('{')
    output_line('  "version": 1,')
    output_line('  "component_list": [')


def output_tailer():
    """Output the component manifest tailer."""
    output_line('')
    output_line('  ]')
    output_line('}')


def output_component(component_type, component_name, i2c_port, i2c_addr, usbc_port):
    """Output the one component inform to the component manifest.

    Args:
        component_type:
        ...
    """
    global first_component
    if not first_component:
        output_line(',')
    else:
        first_component = False

    output_line('    {')
    output_line('      "component_type": "%s",' % component_type)
    output_line('      "component_name": "%s",' % component_name)
    output_line('      "i2c": {')
    output_line('        "port": %d,' % i2c_port)
    output_line('        "addr": "0x%x"' % i2c_addr)
    output_line('      },')
    output_line('      "usbc": {')
    output_line('        "port": %d' % usbc_port)
    output_line('      }')
    output_str('    }')


def find_charger(node, usbc_port):
    """Find the charger node inform.

    Args:
        usbc_port: USB-C port number
    """
    output_component(
            "charger",
            node.props["compatible"].val[0],
            i2c_portmap[node.parent.label],
            node.props["reg"].val[0],
            usbc_port)


def find_bc12(node, usbc_port):
    """Find the BC 1.2 node inform.

    Args:
        usbc_port: USB-C port number
    """
    output_component(
            "bc12",
            node.props["compatible"].val[0],
            i2c_portmap[node.parent.label],
            node.props["reg"].val[0],
            usbc_port)


def find_ppc(node, usbc_port):
    """Find the PPC node inform.

    Args:
        usbc_port: USB-C port number
    """
    output_component(
            "ppc",
            node.props["compatible"].val[0],
            i2c_portmap[node.parent.label],
            node.props["reg"].val[0],
            usbc_port)


def find_tcpc(node, usbc_port):
    """Find the TCPC node inform.

    Args:
        usbc_port: USB-C port number
    """
    if not hasattr(node, "parent") or node.parent.label not in i2c_portmap:
        logging.info('Component %s not on I2C bus, skip it', node.props["compatible"].val[0])
        return

    output_component(
            "tcpc",
            node.props["compatible"].val[0],
            i2c_portmap[node.parent.label],
            node.props["reg"].val[0],
            usbc_port)


def find_usbc(edtlib, edt):
    """EDIT IT: Verify there are no duplicate GPIOs in the named-gpios node.

    Args:
        edtlib: Module object for the edtlib library.
        edt: EDT object representation of a devicetree

    Returns:
        True if no duplicates found.  Returns False otherwise.
    """
    # Dictionary of GPIO controllers, indexed by the GPIO controller nodelabel
    try:
        usbc = edt.get_node("/usbc")
    except edtlib.EDTError:
        # If the usbc node doesn't exist, return success.
        logging.error("Not exist usbc")
        return True

    output_header()
    for node in usbc.children.values():
        if "reg" not in node.props:
            continue

        port = node.props["reg"].val[0]
        if "chg" in node.props:
            chg = node.props["chg"].val
            find_charger(chg, port)

        if "bc12" in node.props:
            bc12 = node.props["bc12"].val
            find_bc12(bc12, port)

        if "ppc" in node.props:
            ppc = node.props["ppc"].val
            find_ppc(ppc, port)

        if "tcpc" in node.props:
            tcpc = node.props["tcpc"].val
            find_tcpc(tcpc, port)

    output_tailer()

    return True


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    """The main function.

    Args:
        argv: Optionally, the command-line to parse, not including argv[0].

    Returns:
        Zero upon success, or non-zero upon failure.
    """
    args = parse_args(argv)

    log_format = "%(levelname)s: %(message)s"

    logging.basicConfig(format=log_format, level=args.log_level)

    edtlib, edt, project_name = _load_edt(args.zephyr_base, args.edt_pickle)

    if edtlib is None:
        return 0

    logging.info("Running CME, outputting to %s", args.manifest_file)
    iterate_i2c_ports(edtlib, edt)
    find_usbc(edtlib, edt)

    with open(args.manifest_file, 'w') as f:
        f.write(g_manifest)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configure-time checks for the named-gpios node."""

import importlib
import logging
import pickle
import sys


def _load_edt(zephyr_base, output_dir):
    """Perform a dynamic import of the edtlib module from the Zephyr source.

    Args:
        zephyr_base - pathlib.Path pointing to the Zephyr OS repository.

    Returns:
        None if edtlib module couldn't be loaded or if the edtlib module failed
        to load the edt pickle file.
    """
    logger = logging.getLogger()

    zephyr_devicetree_path = (
        zephyr_base / "scripts" / "dts" / "python-devicetree" / "src"
    )

    # Add Zephyr's python-devicetree into the source path
    sys.path.insert(0, str(zephyr_devicetree_path))

    edtlib_path = zephyr_devicetree_path / "devicetree" / "edtlib.py"

    if not edtlib_path.is_file():
        return None

    # Dynamically import edtlib
    spec = importlib.util.spec_from_file_location(edtlib_path.name, edtlib_path)
    edtlib = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(edtlib)

    edt_pickle = output_dir / "zephyr" / "edt.pickle"

    try:
        with open(edt_pickle, "rb") as edt_file:
            edt = pickle.load(edt_file)
    except edtlib.DTError as err:
        logger.error("devicetree error: %s", err)
        # TODO: raise an error?
        return None

    return edt


def _detect_gpios_mismatches(node_name, prop_name, prop_gpios, board_gpios):
    """Verify that all GPIO entries in a -gpios style property match the flags
    specified in the named-gpios node.

    Args:
        node_name   - The name of the node containing the -gpios style property.
        prop_name   - The name of the -gpios style property.
        prop_gpios  - An edtlib.ControllerAndData class object containing
                      the GPIO tuples found in the property.
        board_gpios - A dictionary that maps gpio port/pin tuples to the GPIO
                      flags. This dictionary is initialized from the children
                      found in the named-gpios devicetree node.

    Returns:
        A tuple indicating how many GPIOs were checked, and how many GPIOs
        had a mismatch.
    """
    logger = logging.getLogger()

    errors = 0
    count = 0

    # The -gpios property may be an array of GPIO tuples
    for gpio in prop_gpios:
        count += 1
        gpio_pin = gpio.data["pin"]

        # The "flags" cell should be only 16-bits
        dt_flags = gpio.data["flags"] & 0xFFFF

        if gpio.controller.labels[0] is not None:
            nodelabel = gpio.controller.labels[0]
        else:
            nodelabel = gpio.controller.name

        if (nodelabel, gpio_pin) not in board_gpios.keys():
            # GPIO not specified in named-gpios
            logger.debug(
                "Warning: property %s/%s = <%s %s 0x%x> not found in named-gpios",
                node_name,
                prop_name,
                nodelabel,
                gpio_pin,
                dt_flags,
            )
            continue

        if dt_flags != board_gpios[(nodelabel, gpio_pin)]:
            errors += 1
            logger.error(
                "ERROR: property %s/%s = <%s %s 0x%x>. Flags don't match named-gpios 0x%x",
                node_name,
                prop_name,
                nodelabel,
                gpio_pin,
                dt_flags,
                board_gpios[(nodelabel, gpio_pin)],
            )

    return count, errors


def verify_no_duplicates(zephyr_base, output_dir):
    """Verify there are no duplicate GPIOs in the named-gpios node.

    Args:
        zephyr_base - pathlib.Path pointing to the Zephyr OS repository.
        output_dir - pathlib.Path pointing to the output directory for the
                     current build.

    Returns:
        True if no duplicates found.  Returns False otherwise.
    """
    logger = logging.getLogger()

    edt = _load_edt(zephyr_base, output_dir)

    # The zmake tests don't provide a real Zephyr project, so skip these
    # checks if the edtlib.py module cannot be found.
    if edt is None:
        return True

    # Dictionary of GPIO controllers, indexed by the GPIO controller nodelabel
    gpio_ctrls = dict()
    duplicates = 0
    count = 0
    named_gpios = edt.get_node("/named-gpios")
    for node in named_gpios.children.values():
        if "gpios" not in node.props:
            continue

        gpios = node.props["gpios"].val
        count += 1

        # edtlib converts a "-gpios" style property to a list of of
        # ControllerAndData objects.  However, the named-gpios node only
        # supports a single GPIO per child, so no need to iterate over
        # the list.
        gpio = gpios[0]
        gpio_pin = gpio.data["pin"]

        # Note that EDT stores the node name (not a nodelabel) in the
        # Node.name property.  Use the nodelabel, if available as the
        # key for gpio_ctrls.
        if gpio.controller.labels[0] is not None:
            nodelabel = gpio.controller.labels[0]
        else:
            nodelabel = gpio.controller.name

        if not nodelabel in gpio_ctrls:
            # Create a dictionary at each GPIO controller
            gpio_ctrls[nodelabel] = dict()

        if gpio_pin in gpio_ctrls[nodelabel]:
            logger.error(
                "Duplicate GPIOs found at nodes: %s and %s",
                gpio_ctrls[nodelabel][gpio_pin],
                node.name,
            )
            duplicates += 1
        else:
            # Store the node name for the new pin
            gpio_ctrls[nodelabel][gpio_pin] = node.name

    if duplicates:
        logger.error("%d duplicate GPIOs found.", duplicates)
    else:
        logger.info("Verified %d GPIOs, no duplicates found", count)

    if duplicates != 0:
        return False

    return True


def verify_gpios_cross_check(zephyr_base, output_dir):
    """Check that GPIO flags used across devices matches.

    Until all drivers are upstream, the Zephyr EC devicetrees specify GPIOs
    in multiple locations: the "named-gpios" node performs the configuration
    of all GPIOs on the board, and individual drivers may also configure GPIOs.

    This routine finds all "*-gpios" style properties in the devicetree and
    cross-checks the flags set by "named-gpios" and ensures they match.

    Args:
        zephyr_base - pathlib.Path pointing to the Zephyr OS repository.
        output_dir - pathlib.Path pointing to the output directory for the
                     current build.

    Returns:
        True if no duplicates found.  Returns False otherwise.
    """

    logger = logging.getLogger()

    edt = _load_edt(zephyr_base, output_dir)

    # The zmake tests don't provide a real Zephyr project, so skip these
    # checks if the edtlib.py module cannot be found.
    if edt is None:
        return True

    # Dictionary using the gpio,pin tuple as the key. Value set to the flags
    board_gpios = dict()
    named_gpios = edt.get_node("/named-gpios")
    for node in named_gpios.children.values():
        if "gpios" not in node.props:
            continue

        gpios = node.props["gpios"].val

        # edtlib converts a "-gpios" style property to a list of of
        # ControllerAndData objects.  However, the named-gpios node only
        # supports a single GPIO per child, so no need to iterate over
        # the list.
        gpio = gpios[0]
        gpio_pin = gpio.data["pin"]

        if gpio.controller.labels[0] is not None:
            nodelabel = gpio.controller.labels[0]
        else:
            nodelabel = gpio.controller.name

        # The named-gpios stores a 32-bit flags, but Zephyr GPIO flags
        # are limited to the lower 16-bits
        board_gpios[(nodelabel, gpio_pin)] = gpio.data["flags"] & 0xFFFF

    errors = 0
    count = 0
    for node in edt.nodes:
        for prop_name in node.props.keys():
            if prop_name.endswith("-gpios"):
                gpios = node.props[prop_name].val

                prop_gpio_count, prop_gpio_errors = _detect_gpios_mismatches(
                    node.name, prop_name, gpios, board_gpios
                )
                count += prop_gpio_count
                errors += prop_gpio_errors

    if errors:
        logger.error("%d GPIO mismatches found.", errors)
        return False

    logger.info("Verified %d '*-gpios' properties, all flags match", count)
    return True

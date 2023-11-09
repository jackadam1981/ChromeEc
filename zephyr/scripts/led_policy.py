# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configure-time checks for the led-policy node."""

import logging
import sys
from typing import List, Optional

from scripts import util


def check_policy(charge_state, charge_port, chipset_state, batt_lvl, policies):
    """Checks if a given state has valid policy coverage

    Args:
        charge_state: charge state to be tested
        charge_port: charge port to be tested
        chipset_state: chipset state to be tested
        batt_lvl: battery level to be tested

    Return:
        A boolean: True if state is in the LED policies, false if not.
    """
    for node in policies.children.values():
        # check_policy is a replica of the match_node() function in led.c
        if "charge-state" in node.props:
            if charge_state != node.props["charge-state"].val:
                continue

            if "charge-port" in node.props:
                if charge_port != node.props["charge-port"].val:
                    continue

        if "chipset-state" in node.props:
            if chipset_state != node.props["chipset-state"].val:
                continue

        if "batt-lvl" in node.props:
            if (
                batt_lvl < node.props["batt-lvl"].val[0]
                or batt_lvl > node.props["batt-lvl"].val[1]
            ):
                continue

        return True

    # None of the devicetree nodes match this combination of states
    return False


def iterate_power_states(edtlib, edt):
    """Iterate all combinations of states and test the led policy coverage.

    Args:
        edtlib: Module object for the edtlib library.
        edt: EDT object representation of a devicetree
    """
    try:
        policies = edt.get_node("/led-colors")
    except edtlib.EDTError:
        # If the led-colors node doesn't exist, return success.
        logging.error("No led-colors node found")
        return

    charge_state_list = [
        "LED_PWRS_CHARGE",
        "LED_PWRS_DISCHARGE",
        "LED_PWRS_ERROR",
        #       "LED_PWRS_IDLE", often unsupported
        "LED_PWRS_FORCED_IDLE",
        "LED_PWRS_CHARGE_NEAR_FULL",
    ]

    try:
        usbc = edt.get_node("/usbc")
    except edtlib.EDTError:
        # If the usbc node doesn't exist, return success.
        logging.error("No usbc node found")
        return

    charge_port_num = 0
    for port in usbc.children.values():
        if "reg" in port.props:
            charge_port_num += 1

    chipset_state_list = [
        "POWER_S0",
        "POWER_S3",
        "POWER_S5",
    ]

    # no Zephyr project currectly uses batt_state to determine LED policy

    for charge_state in charge_state_list:
        for charge_port in range(charge_port_num):
            for chipset_state in chipset_state_list:
                batt_max = -1
                batt_range = 0
                for batt_lvl in range(101):
                    if not check_policy(
                        charge_state,
                        charge_port,
                        chipset_state,
                        batt_lvl,
                        policies,
                    ):
                        batt_range += 1
                        batt_max = batt_lvl

                if batt_range == 101:
                    logging.error(
                        "No LED policy found for %s, port %i, %s",
                        charge_state,
                        charge_port,
                        chipset_state,
                    )
                elif batt_range != 0:
                    logging.error(
                        "Missing battery level at %i%% for %s, port %i, %s",
                        batt_max,
                        charge_state,
                        charge_port,
                        chipset_state,
                    )


def parse_args(argv: Optional[List[str]] = None):
    """Returns parsed command-line arguments"""
    parser = util.EdtArgumentParser(
        prog="led_policy_check",
        description="Zephyr EC specific devicetree checks",
    )

    return parser.parse_args(argv)


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

    edtlib, edt, _unused = util.load_edt(args.zephyr_base, args.edt_pickle)

    if edtlib is None:
        return 0

    iterate_power_states(edtlib, edt)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

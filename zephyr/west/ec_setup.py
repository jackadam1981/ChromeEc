# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" west setup command extension """

from textwrap import dedent  # just for nicer code indentation

from ec_env_builder import EcEnvBuilder
from west.commands import WestCommand  # pylint: disable=import-error


class ECSetup(WestCommand):
    """
    west ec setup command
      Creates a virtual environment for Zephyr EC
      Downloads and installs Zephyr SDK Tools
    """

    def __init__(self):
        super().__init__(
            "ec-setup",  # gets stored as self.name
            "Setup EC build environment",  # self.help
            # self.description:
            dedent(
                """
            Sets up EC build environment"""
            ),
        )

    def do_add_parser(self, parser_adder):
        """
        Add additional arguments for setup command.
        """
        parser = parser_adder.add_parser(
            self.name, help=self.help, description=self.description
        )
        parser.add_argument(
            "-v", "--verbose", action="store_true", help="verbose logging"
        )

        return parser

    def do_run(self, args, unknown_args):  # pylint: disable=no-self-use
        """
        Run setup command
        """
        del unknown_args
        builder = EcEnvBuilder(verbose=args.verbose)
        builder.create(".venv")

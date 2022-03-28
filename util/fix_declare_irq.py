#!/usr/bin/env python3

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import re
import sys
from typing import List

"""
To fix all instances:
git grep --name-only 'DECLARE_IRQ' | xargs -I {} ./util/fix_declare_irq.py -f {}
"""

DECLARE_IRQ_PATTERN = re.compile(r'^DECLARE_IRQ\([\w_()]+, ([\w_]+)(, [\w]+)?\);', re.DOTALL)


def find_declare_irq_functions(text: list) -> List[str]:
    ret = []

    for line in text:
        match = DECLARE_IRQ_PATTERN.match(line)
        if match:
            logging.debug('Found match: %s', match.group(1))
            ret = ret + [match.group(1)]

    logging.debug('Returning: %s', ret)
    return ret


def fix_irq_handler(irq_functions: list, text: list) -> str:
    ret = ''.join(text)

    for function_name in irq_functions:
        irq_handler_pattern = re.compile('static void ' + function_name + r'\(void\)', re.DOTALL)
        logging.debug('irq_handler_pattern: %s', irq_handler_pattern)
        ret = irq_handler_pattern.sub(r'static void __keep ' + function_name + r'(void)', ret)

    return ret


def main() -> int:
    parser = argparse.ArgumentParser()

    log_level_choices = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    parser.add_argument(
        '--log_level', '-l',
        choices=log_level_choices,
        default='DEBUG'
    )

    parser.add_argument(
        '--file', '-f',
    )

    args = parser.parse_args()
    logging.basicConfig(level=args.log_level)

    logging.debug('Modifying file: %s' % args.file)
    with open(args.file, 'r') as f:
        lines = f.readlines()
        function_names = find_declare_irq_functions(lines)
        ret = fix_irq_handler(function_names, lines)

    with open(args.file, 'w') as f:
        f.write(ret)

    return 0


if __name__ == '__main__':
  sys.exit(main())

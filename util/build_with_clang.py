#!/usr/bin/env python3

# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build firmware with clang instead of gcc."""
import argparse
import concurrent
import logging
import multiprocessing
import os
import subprocess
import sys

from concurrent.futures import ThreadPoolExecutor

# Add to this list as compilation errors are fixed for boards.
BOARDS_THAT_COMPILE_SUCCESSFULLY_WITH_CLANG = [
    'dartmonkey',
    'bloonchipper',
    'nucleo-f412zg',
    'nucleo-h743zi',
    'zinger',
    'minimuffin',
    'servo_micro',
    'servo_v4p1',
    'c2d2',
    'dingdong',
    'discovery-stm32f072',
    'hoho',
    'nucleo-f072rb',
    'pdeval-stm32f072',
    'plankton',
    'tigertail',
    'twinkie',
    'coffeecake',
    'polyberry',
    'stm32f446e-eval',
    'sweetberry',
    'baklava',
    'gingerbread',
    'nucleo-g431rb',
    'quiche',
    'nucleo-f411re',
    'ambassador',
    'anahera',
    'boldar',
    'brask',
    'brya',
    'chronicler',
    'collis',
    'copano',
    'dalboz',
    'delbin',
    'dirinboz',
    'dooly',
    'drobit',
    'eldrid',
    'elemi',
    'felwinter',
    'genesis',
    'gimble',
    'gumboz',
    'kano',
    'kohaku',
    'lindar',
    'metaknight',
    'moonbuggy',
    'morphius',
    'nightfury',
    'primus',
    'puff',
    'redrix',
    'scout',
    'taeko',
    'trembyle',
    'voema',
    'volet',
    'voxel',
    'woomax',
# The following compile, but run out of space:
    #'fennel',
    #'kodama',
    #'makomo',
    #'halvor',
    #'malefor',
    #'terrador',
    #'lingcod',
    #'volteer',
    #'todor',
    #'trondo',
    #'servo_v4',
    #'jacuzzi',
    #'stern',
    #'burnet',
    #'damu',
    #'willow',
    #'oak',
    #'kappa',         # enough space when reverting use of compiler-rt
    #'fusb307bgevb',  # enough space when reverting use of compiler-rt
# Other errors:
    #'fluffy',
# ld.lld: error: undefined symbol: __aeabi_memclr4
    #'rainier',
    #'scarlet',
    #'elm',
# ld.lld: error: undefined symbol: __atomic_load_4
    #'chocodile_vpdmcu',
]


def build(board_name: str) -> None:
    """Build with clang for specified board."""
    logging.debug('Building board: "%s"', board_name)

    cmd = [
        'make',
        'BOARD=' + board_name,
        '-j',
    ]

    logging.debug('Running command: "%s"', ' '.join(cmd))
    subprocess.run(cmd, env=dict(os.environ, CC='clang'), check=True)


def main() -> int:
    parser = argparse.ArgumentParser()

    log_level_choices = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    parser.add_argument(
        '--log_level', '-l',
        choices=log_level_choices,
        default='DEBUG'
    )

    parser.add_argument(
        '--num_threads', '-j',
        type=int,
        default=multiprocessing.cpu_count()
    )

    args = parser.parse_args()
    logging.basicConfig(level=args.log_level)

    logging.debug('Building with %d threads', args.num_threads)

    failed_boards = []
    with ThreadPoolExecutor(max_workers=args.num_threads) as executor:
        future_to_board = {
            executor.submit(build, board): board
            for board in BOARDS_THAT_COMPILE_SUCCESSFULLY_WITH_CLANG
        }
        for future in concurrent.futures.as_completed(future_to_board):
            board = future_to_board[future]
            try:
                future.result()
            except Exception:
                failed_boards.append(board)

    if len(failed_boards) > 0:
        logging.error('The following boards failed to compile:\n%s',
                      '\n'.join(failed_boards))
        return 1

    logging.info('All boards compiled successfully!')
    return 0


if __name__ == '__main__':
    sys.exit(main())

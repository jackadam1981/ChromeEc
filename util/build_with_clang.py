#!/usr/bin/env python3

# Copyright 2021 The ChromiumOS Authors
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
    "dartmonkey",
    "bloonchipper",
    "nucleo-f412zg",
    "nucleo-h743zi",
]

<<<<<<< HEAD   (4df964 utils: remove ectool from dedede firmware branch)
=======
NDS32_BOARDS = [
    "adlrvpm_ite",
    "adlrvpp_ite",
    "ampton",
    "beadrix",
    "beetley",
    "blipper",
    "boten",
    "dibbi",
    "drawcia",
    "galtic",
    "gooey",
    "haboki",
    "it83xx_evb",
    "kracko",
    "lantis",
    "pirika",
    "reef_it8320",
    "sasukette",
    "shotzo",
    "storo",
    "waddledee",
    "wheelie",
]

RISCV_BOARDS = [
    "asurada",
    "asurada_scp",
    "cherry",
    "cherry_scp",
    "cozmo",
    "dojo",
    "drawcia_riscv",
    "goroh",
    "hayato",
    "icarus",
    "it8xxx2_evb",
    "it8xxx2_pdevb",
    "pico",
    "spherion",
    "tomato",
]

BOARDS_THAT_FAIL_WITH_CLANG = [
    # Boards that use CHIP:=stm32 and *not* CHIP_FAMILY:=stm32f0
    "bellis",  # overflows flash
    "munna",  # overflows flash
    # Boards that use CHIP:=stm32 *and* CHIP_FAMILY:=stm32f0
    "burnet",  # overflows flash
    "cerise",  # overflows flash
    "chocodile_vpdmcu",  # compilation error: b/254710459
    "damu",  # overflows flash
    "fennel",  # overflows flash
    "jacuzzi",  # overflows flash
    "juniper",  # overflows flash
    "kakadu",  # overflows flash
    "kappa",  # overflows flash
    "katsu",  # overflows flash
    "kodama",  # overflows flash
    "krane",  # overflows flash
    "kukui",  # overflows flash
    "makomo",  # overflows flash
    "oak",  # overflows flash
    "servo_v4",  # overflows flash
    "stern",  # overflows flash
    "willow",  # overflows flash
    # Boards that use CHIP:=mchp
    # git grep --name-only 'CHIP:=mchp' | sed 's#board/\(.*\)/build.mk#"\1",#'
    "adlrvpp_mchp1521",  # overflows flash
    # Boards that use CHIP:=npcx
    "garg",  # overflows flash
    "gelarshie",  # overflows flash
    "mushu",  # overflows flash
    "nocturne",  # overflows flash
    "terrador",  # overflows flash
    "volteer",  # overflows flash
    "waddledoo",  # overflows flash
]

# TODO(b/201311714): NDS32 is not supported by LLVM.
BOARDS_THAT_FAIL_WITH_CLANG += NDS32_BOARDS
# TODO(b/201310017): RISC-V is not supported in our LLVM toolchain.
BOARDS_THAT_FAIL_WITH_CLANG += RISCV_BOARDS

>>>>>>> CHANGE (402869 dibbi: Create initial EC image)

def build(board_name: str) -> None:
    """Build with clang for specified board."""
    logging.debug('Building board: "%s"', board_name)

    cmd = [
        "make",
        "BOARD=" + board_name,
        "-j",
    ]

    logging.debug('Running command: "%s"', " ".join(cmd))
    subprocess.run(cmd, env=dict(os.environ, CC="clang"), check=True)


def main() -> int:
    parser = argparse.ArgumentParser()

    log_level_choices = ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
    parser.add_argument(
        "--log_level", "-l", choices=log_level_choices, default="DEBUG"
    )

    parser.add_argument(
        "--num_threads", "-j", type=int, default=multiprocessing.cpu_count()
    )

    args = parser.parse_args()
    logging.basicConfig(level=args.log_level)

    logging.debug("Building with %d threads", args.num_threads)

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
        logging.error(
            "The following boards failed to compile:\n%s",
            "\n".join(failed_boards),
        )
        return 1

    logging.info("All boards compiled successfully!")
    return 0


if __name__ == "__main__":
    sys.exit(main())

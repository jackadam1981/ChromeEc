#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
docstring
"""

import argparse
import math


class Obuf:
    """
    docstring
    """

    def __init__(self):
        self.linebuf = ""
        self.items = 0
        self.max_items = 4

    def write(self, my_str):
        """
        docstring
        """

        if my_str:
            if self.items == 0:
                self.linebuf = "\t"
            else:
                self.linebuf += " "
            self.linebuf += my_str
            self.items += 1

        if (not my_str and self.items > 0) or (self.items >= self.max_items):
            print(self.linebuf)
            self.linebuf = ""
            self.items = 0

    def flush(self):
        """
        docstring
        """

        self.write(None)


def gamma_compensation(in_val, min_val=0, max_val=100, gamma=1.0):
    """
    Compute gamma compensation factor.
    A gamma of 1 is the identity function.
    A gamma > 1 decreases response at lower values and increases
    response at higher values.
    """

    if in_val < min_val:
        return min_val
    if in_val > max_val:
        return max_val

    val_range = max_val - min_val
    return (in_val / val_range) ** gamma * val_range


def gen_cosine(points, my_min=0, my_max=255, phase=0, verbose=False, gamma=1.0):
    """
    Generate non-negative integer point values
    for a single cosine wave. The output can be
    used to initialize a C array.

    Args:
      my_min:    minimum value (typically 0)
      my_max:    maximum value (typically 255)
      points: number of discrete points on wave
      phase:  phase shift in radians.
    """

    obob = Obuf()

    for ptpt in range(points):
        rads = 2 * math.pi * ptpt / points
        rads += phase
        val = (1 + math.cos(rads)) / 2
        v_range = my_min + val * (my_max - my_min)
        g_range = gamma_compensation(v_range, my_min, my_max, gamma)

        if verbose:
            print("cos point  %3d: %1.3f" % (ptpt, val))
            print("value           %1.3f" % v_range)
            print("gamma corrected %1.3f" % g_range)

        obob.write("[%3d] = %3u," % (ptpt, g_range))

    obob.flush()


def main():
    """Main function."""

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument(
        "-v", "--verbose", action="store_true", help="verbose output"
    )
    arg_parser.add_argument(
        "--min", type=int, action="store", default=0, help="min value"
    )
    arg_parser.add_argument(
        "--max", type=int, action="store", default=255, help="max value"
    )
    arg_parser.add_argument(
        "--points",
        type=int,
        action="store",
        default=16,
        help="number of points",
    )
    arg_parser.add_argument(
        "--phase",
        type=int,
        action="store",
        default=0,
        help="phase offset of in degrees",
    )
    arg_parser.add_argument(
        "--gamma",
        type=float,
        action="store",
        default=1.0,
        help="gamma compensation",
    )
    args = arg_parser.parse_args()

    phase = args.phase / 180 * math.pi

    gen_cosine(
        args.points,
        args.min,
        args.max,
        phase=phase,
        verbose=args.verbose,
        gamma=args.gamma,
    )


if __name__ == "__main__":
    main()

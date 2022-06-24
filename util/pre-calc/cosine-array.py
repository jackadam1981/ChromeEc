#!/usr/bin/env python3
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import math
import sys

verbose = False


class obuf(object):
    def __init__(self):
        self.linebuf = ''
        self.items = 0
        self.max_items = 4

    def write(self, str):
        if str:
            if self.items == 0:
                self.linebuf = '\t'
            else:
                self.linebuf += ' '
            self.linebuf += str
            self.items += 1

        if (not str and self.items > 0) or (self.items >= self.max_items):
            print(self.linebuf)
            self.linebuf = ''
            self.items = 0

    def flush(self):
        self.write(None)


def gen_cosine(points, min = 0, max = 255, phase = 0):
    """
    Generate non-negative integer point values
    for a single cosine wave. The output can be
    used to initialize a C array.

    Args:
      min:    minimum value (typically 0)
      max:    maximum value (typically 255)
      points: number of discrete points on wave
      phase:  phase shift in radians.
    """

    ob = obuf()

    for p in range(points):
        rads = 2 * math.pi * p / points
        rads += phase
        v = (1 + math.cos(rads)) / 2

        if verbose:
            print("point %3d: %1.3f" % (p, v))

        v_int = min + v * (max - min)
        ob.write("[%3d] = %3u," % (p, v_int))

    ob.flush()

def main():
    """Main function."""

    global verbose

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument('-v', '--verbose',
                            action='store_true',
                            help='verbose output')
    arg_parser.add_argument('--min',
                            type=int,
                            action='store',
                            default=0,
                            help='min value')
    arg_parser.add_argument('--max',
                            type=int,
                            action='store',
                            default=255,
                            help='max value')
    arg_parser.add_argument('--points',
                            type=int,
                            action='store',
                            default=16,
                            help='number of points')
    arg_parser.add_argument('--phase',
                            type=int,
                            action='store',
                            default=0,
                            help='phase offset of in degrees')
    args = arg_parser.parse_args()

    if args.verbose == True:
        verbose = True

    phase = args.phase / 180 * math.pi

    gen_cosine(args.points, args.min, args.max, phase = phase)


if __name__ == '__main__':
    main()

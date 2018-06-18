#!/usr/bin/python
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Program to convert power logging config from a servo_ina device
   to a sweetberry config.
"""


import json
import os
import sys

# Map between header channel number (0-47)
# and INA I2C bus/addr on sweetberry.
CHMAP = {
    0: (3, 0x40),
    1: (1, 0x40),
    2: (2, 0x40),
    3: (0, 0x40),
    4: (3, 0x41),
    5: (1, 0x41),
    6: (2, 0x41),
    7: (0, 0x41),
    8: (3, 0x42),
    9: (1, 0x42),
    10: (2, 0x42),
    11: (0, 0x42),
    12: (3, 0x43),
    13: (1, 0x43),
    14: (2, 0x43),
    15: (0, 0x43),
    16: (3, 0x44),
    17: (1, 0x44),
    18: (2, 0x44),
    19: (0, 0x44),
    20: (3, 0x45),
    21: (1, 0x45),
    22: (2, 0x45),
    23: (0, 0x45),
    24: (3, 0x46),
    25: (1, 0x46),
    26: (2, 0x46),
    27: (0, 0x46),
    28: (3, 0x47),
    29: (1, 0x47),
    30: (2, 0x47),
    31: (0, 0x47),
    32: (3, 0x48),
    33: (1, 0x48),
    34: (2, 0x48),
    35: (0, 0x48),
    36: (3, 0x49),
    37: (1, 0x49),
    38: (2, 0x49),
    39: (0, 0x49),
    40: (3, 0x4a),
    41: (1, 0x4a),
    42: (2, 0x4a),
    43: (0, 0x4a),
    44: (3, 0x4b),
    45: (1, 0x4b),
    46: (2, 0x4b),
    47: (0, 0x4b),
}


def fetch_records(board_file):
    """Import records from servo_ina file.

    board files are python imports, and have a list of tuples with
    the INA data.
    (name, rs, swetberry_num, net_name, channel)

    Args:
      board_file: board file

    Returns:
      list of tuples as described above.
    """
    data = None
    with open(board_file) as f:
        data = json.load(f)
    return data


def main(argv):
    if len(argv) != 2:
        print "usage:"
        print " %s input.board" % argv[0]
        return

    inputf = argv[1]
    basename = os.path.splitext(inputf)[0]
    outputf = basename + '.py'

    print "Converting %s to %s" % (inputf, outputf)

    inas = fetch_records(inputf)

    pyfile = open(outputf, 'w')

    pyfile.write('inas = [\n')
    start = True

    for rec in inas:
        if start:
            start = False
        else:
            pyfile.write(',\n')
        #('sweetberry', 0x40, 'SB_FW_CAM_2P8', 5.0, 1.000, 3, False),
        i2c_addr = CHMAP[rec['channel']][1]
        channel = CHMAP[rec['channel']][0]
        record = "    ('sweetberry', 0x%02x, '%s', 5.0, %f, %d, 'False')" % (
            i2c_addr, rec['name'], rec['rs'], channel)
        pyfile.write(record)

    pyfile.write('\n')
    pyfile.write(']\n')


if __name__ == "__main__":
    main(sys.argv)

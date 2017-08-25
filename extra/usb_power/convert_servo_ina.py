#!/usr/bin/python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Program to convert power logging config from a servo_ina device
   to a sweetberry config.
"""


import os
import sys


def fetch_records(basename):
  ina_desc = __import__(basename)
  return ina_desc.inas


def main(argv):
  if len(argv) != 2:
    print "usage:"
    print " %s input.py" % argv[0]
    return

  inputf = argv[1]
  basename = os.path.splitext(inputf)[0]
  outputf = basename + '.board'
  outputs = basename + '.scenario'

  print "Converting %s to %s,%s" % (inputf, outputf, outputs)

  inas = fetch_records(basename)


  o = open(outputf, 'w')
  s = open(outputs, 'w')

  o.write('[\n')
  s.write('[\n')
  start = True

  for rec in inas:
    if start:
      start = False
    else:
      o.write(',\n')
      s.write(',\n')

    record = '  {"name": "%s", "rs": %f, "sweetberry": "A", "channel": %d}' % (
                         rec[2],    rec[4],                       rec[1] - 64)
    o.write(record)
    s.write('"%s"' % rec[2])

  o.write('\n')
  o.write(']')

  s.write('\n')
  s.write(']')

if __name__ == "__main__":
  main(sys.argv)

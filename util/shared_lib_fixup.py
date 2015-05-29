#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fixup RW linker script.

Script to fixup the RW linker script with the address and length of the
shared objects library (libsharedobjs) present in RO.
"""
from __future__ import print_function
import os
import re

if len(os.sys.argv) < 4:
  print("Usage: %s outdir project(stem match) skip_lib V\n" % os.sys.argv[0])
  quit()

OUTDIR = os.sys.argv[1]
STEM = os.sys.argv[2]
SKIP_LIB = os.sys.argv[3]
if len(os.sys.argv) >= 5:
  VERBOSE = int(os.sys.argv[4])
else:
  VERBOSE = 0

if VERBOSE == 1:
  print(os.sys.argv)

DIR = os.path.join(OUTDIR, os.path.dirname(STEM))
BASENAME = os.path.basename(STEM)
PROJECT = BASENAME.split('.')[0]

if VERBOSE == 1:
  print(' outdir: %s\n stem: %s\n skip_lib: %s' % (OUTDIR, STEM, SKIP_LIB))
  print(' dir: %s\n basename: %s\n proj: %s' % (DIR, BASENAME, PROJECT))

rw_linker_script = open(os.path.join(DIR, BASENAME + '.lds'), 'r')
tmp_out_file = open(os.path.join(DIR, BASENAME + '.lds.tmp'), 'w')

if VERBOSE == 1:
  print('Fixing up %s...' % rw_linker_script.name)

shared_lib_is_empty = True

if PROJECT == "ec" and SKIP_LIB == "no":
  # Read the map file and determine the address of the .roshared section
  ro_map_file = open(os.path.join(OUTDIR, 'RO', PROJECT + '.RO.map'), 'r')
  for line in ro_map_file:
    if '.roshared' in line:
      # Read in the address and length from the map file.
      data = re.split(r'\s+', line)
      # Check to see if there's anything in the .roshared section
      if len(data) < 3:
        break
      else:
        shared_lib_is_empty = False
        load_address = data[1]
        length = data[2]
        if VERBOSE == 1:
          print(' .roshared section is NOT empty.')
          print('   loc=%08x len=%08x' % (int(load_address,16), int(length,16)))
        break
  ro_map_file.close()

if shared_lib_is_empty:
  load_address = "0x00000000"
  length = "0x00000000"
  if VERBOSE == 1:
    print(' .roshared section is empty. Setting address and' + \
          ' length to 0.')

for line in rw_linker_script:
  if 'SHARED_LIB (rx)' in line:
    # Fixup the address and length for the shared lib
    line = re.sub('FIXUP_ADDR', load_address, line, flags=re.M)
    if VERBOSE != 1:
      print('  FIXUP   %s' % STEM + '.lds')
    tmp_out_file.write(re.sub('FIXUP_LENGTH', length, line, flags=re.M))
    if shared_lib_is_empty != True:
      print('The shared object library (size: %s bytes) is present at %s' % \
            (length, load_address))
    continue
  tmp_out_file.write(line)

rw_linker_script.close()
tmp_out_file.close()

# Now, save the fixed up version of the linker script.
try:
  os.rename(tmp_out_file.name, rw_linker_script.name)
except OSError:
  print("%s does not exist!" % tmp_out_file.name)

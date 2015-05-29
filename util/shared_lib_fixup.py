# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#!/usr/bin/python
"""Script to fixup the RW linker script with the address and length of the
shared objects library present in RO.
"""
import os
import re

if len(os.sys.argv) < 5:
    print "Usage: %s outdir lib project(stem match) skip_lib\n" % os.sys.argv[0]
    quit()

OUTDIR = os.sys.argv[1]
LIB = os.sys.argv[2]
STEM = os.sys.argv[3]
skip_lib = os.sys.argv[4]

LIB_PATH = OUTDIR + '/' + LIB + '/'
DIR = OUTDIR + '/' + os.path.dirname(STEM) + '/'
BASENAME = os.path.basename(STEM)
PROJECT = BASENAME.split('.')[0]

rw_linker_script = open(DIR + BASENAME + '.lds', 'r')
tmp_out_file = open(DIR + BASENAME + '.lds.tmp', 'w')

shared_lib_is_empty = True

if PROJECT == "ec" and skip_lib == "no":
    # Read the map file and determine the address of the .roshared section
    ro_map_file = open(OUTDIR + '/RO/' + PROJECT + '.RO.map','r')
    for line in ro_map_file:
        if '.roshared' in line:
            # Read in the address and length from the map file.
            data = re.split('\s+', line)
            # Check to see if there's anything in the .roshared section
            if len(data) < 3:
                break
            else:
                shared_lib_is_empty = False
                load_address = data[1]
                length = data[2]
                break
    ro_map_file.close()

if shared_lib_is_empty:
    load_address = "0x000000000"
    length = "0x00000000"

for line in rw_linker_script:
    if 'SHARED_LIB (rx)' in line:
        # Fixup the address and length for the shared lib
        line = re.sub('FIXUP_ADDR', load_address, line, flags=re.M)
        print('  FIXUP   %s' % STEM + '.lds')
        tmp_out_file.write(re.sub('FIXUP_LENGTH', length, line, flags=re.M))
        if shared_lib_is_empty != True:
            print('The shared object library (size: %s bytes) is present at %s' % (length, load_address))
        continue
    tmp_out_file.write(line)

rw_linker_script.close()
tmp_out_file.close()

# Now, save the fixed up version of the linker script.
try:
    os.rename(tmp_out_file.name, rw_linker_script.name)
except OSError:
    print "%s does not exist!" % tmp_out_file.name

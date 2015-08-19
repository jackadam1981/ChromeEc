#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared Objects Library Version Packer

Script to pack in the version info into a shared library flat binary.

It copies the ELF file from the output build directory and places it in the
board's libsharedobjs directory along with the flat binary with the appropriate
header.  This header includes the version number and the SHA1 sum of the library
itself (excluding the version info).

"""

from __future__ import print_function
import os
import re
import subprocess
import binascii
import hashlib
import glob

# TODO(aaboagye): Add nice errors and such. :D

LSO_VERSION_FILE = '.libsharedobjs_version'
# The version is 24 bytes long.  3 bytes for the numbers, and 20-bytes for the
# sha1 checksum and a 1 byte pad for alignment.
VERSION_REGION_LENGTH = 24

def main():

  if len(os.sys.argv) != 2:
    print('Usage: %s <board name>' % os.sys.argv[0])
    quit()
  board = os.sys.argv[1]
  lso = os.path.join('build', board, 'libsharedobjs', 'libsharedobjs.flat')
  lso_version_re = re.compile('^([0-9]+).([0-9]+).([0-9]+)$')
  major_version = 0
  minor_version = 0
  fix_num = 0

  # TODO(aaboagye) Test if the board exists
  print('Packing version info for shared lib on %s...' % board)

  # Open the version file and get the current version.
  with open(LSO_VERSION_FILE, 'r') as version_file:
    for line in version_file:
      match = lso_version_re.search(line)
      if not match:
        raise ValueError(('Incorrectly formatted version file.'
                          'The version should be specified as '
                          'major.minor.fix_num.\n'
                          'ex: 0.1.0'))
      else:
        # Extract the version numbers.
        major_version = int(match.groups()[0])
        minor_version = int(match.groups()[1])
        fix_num = int(match.groups()[2])
        print('LSO version: %d.%d.%d' % (major_version,
                                                minor_version,
                                                fix_num))

  # Take sha1sum of shared lib for the board.
  with open(lso, 'r') as shared_lib:
    shared_lib.seek(VERSION_REGION_LENGTH)
    lib_bin = shared_lib.read()
  checksum = hashlib.sha1(lib_bin).hexdigest()
  print('SHA1: %s\n' % checksum)
  checksum = binascii.unhexlify(checksum)

  # Create the header with the version info.
  new_lib_filename = ('libsharedobjs-' + str(major_version) + '.'
                      + str(minor_version) + '.' + str(fix_num))
  with open(new_lib_filename + '.hdr', 'wb') as lib:
    version = bytearray(3)
    version[0] = major_version
    version[1] = minor_version
    version[2] = fix_num
    lib.write(version)
    lib.write(checksum)
    lib.write(bytearray(1)) # 1 byte pad to align to 24B.

  # Copy the library, discarding the empty compiled header.
  cmd = ('dd bs=1 skip=' + str(VERSION_REGION_LENGTH) + ' if=' + lso + ' of=' +
         new_lib_filename + '.lib').split(' ')
  subprocess.call(cmd)

  # Copy the ELF to the board directory.  This is needed for linking, but the
  # flat image will be actually packed in the EC image.
  cmd = ('mkdir -p ' + os.path.join('board', board, 'libsharedobjs')).split(' ')
  subprocess.call(cmd)
  cmd = ('cp ' + os.path.join('build', board, 'libsharedobjs',
                              'libsharedobjs.elf ') +
         os.path.join('board', board, 'libsharedobjs',
                      new_lib_filename + '.elf')).split(' ')
  subprocess.call(cmd)

  # Build the final flat binary with the new header.
  with open(new_lib_filename + '.flat', 'wb') as new_lib:
    with open(new_lib_filename + '.hdr', 'r') as header:
      data = header.read()
      new_lib.write(data)
    with open(new_lib_filename + '.lib', 'r') as raw_lib:
      data = raw_lib.read()
      new_lib.write(data)

  # Move binary to board directory
  cmd = ('mv ' + new_lib_filename + '.flat ' + os.path.join('board', board,
                                                            'libsharedobjs',
                                                           new_lib_filename +
                                                           '.flat')).split(' ')
  subprocess.call(cmd)

  # Clean up temporary files.
  temp_files = glob.glob(new_lib_filename + '*')
  for f in temp_files:
    os.remove(f)
  print('%s created.' % (os.path.join('board', board,
                                      new_lib_filename + '.flat')))

  # TODO(aaboagye): Do we want to do any verification here? It seems that it's
  # doing the right thing though.

if __name__ == '__main__':
  main()

#!/bin/bash
#
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# compare local tree Cr50 source files with a different git branch.
#
# This script is supposed to run in a Cr50 source tree. The script will remove
# and rebuild Cr50 image, then scan the .o and .d files in the build directory
# to figure out all *.[cSh] files used to build the image.
#
# Once the list of files is created, all files' contents are compared with a
# different git branch. By default the other branch is
# cros/firmware-cr50-9308.B, it can be changed as a command line argument
# passed to this script.

tmpf="/tmp/bcmp.$$"
trap '{ rm -f "${tmpf}" ; }' EXIT

find_src_file() {
  local f="$1"
  local log="$2"

  if [[ -f $f ]]; then
    echo "${cr50_dir} ${f}" >> "${log}"
    return
  fi

  g="${f/\.c/.S}"
  if [[ "$g" != "$f" ]]; then
    # Could be an assembler source file.
    if [[ -f $g ]]; then
      echo "${cr50_dir} $g" >> "${log}"
      return
    fi
  fi
  g="${f#board/cr50/}"
  if [[ -f ${tpm2_dir}/$g ]]; then
    echo "${tpm2_dir} $f" >> "${log}"
    return
  fi
  case "$f" in
    (*dcrypto/fips_module* | *linkedtpm2*) ;;
    (*) echo "neither $f nor $g found in this tree" >&2
  esac
}

if [[ ! -f Makefile.rules || ! -f board/cr50/board.h ]]; then
  echo "this script must run in the root ec directory" >&2
  exit 1
fi

if [[ "$#" != 0 ]]; then
  compare_to="$1"
else
  compare_to="cros/firmware-cr50-9308.B"
fi

cr50_dir="$(pwd)"
tpm2_dir="$(readlink -f ../../third_party/tpm2/)"

for d in "${cr50_dir}" "${tpm2_dir}"; do
  if ! git -C "${d}" show-branch "${compare_to}" > /dev/null 2>&1; then
    echo "Branch ${compare_to} not found in ${d}" >&2
    exit 1
  fi
done

echo "Will rebuild CR50"
rm -rf build/cr50
if ! make BOARD=cr50 -j > /dev/null ; then
  echo "building cr50 failed" >&2
  exit 1
fi

echo "Now checking .c and .S files"
for f in $(find build/cr50/RW/ -name '*.o' |
             sed 's|build/cr50/RW/||;s/\.o/.c/'); do
  find_src_file "$f" "${tmpf}"
done

echo "Now checking .h files"
for f in $(find build/cr50/RW/ -name '*.d' -exec cat {} + |
             sed 's/ /\n/g' |
             sed 's^/mnt/host/source/src/platform/\(ec\|cr50\)/^^' |
             sed 's^/mnt/host/source/src/third_party/tpm2/^^' |
             sed 's/:$//'|
             sort -u |
             grep '\.h$' ); do
  find_src_file "$f" "${tmpf}"
done

sort "${tmpf}" | while read -r dir file; do
  if ! git -C "${dir}" diff "${compare_to}" "${file}" 2>/dev/null; then
    echo "Problems comparing ${file}, does it exist in ${compare_to}?" >&2
  fi
done

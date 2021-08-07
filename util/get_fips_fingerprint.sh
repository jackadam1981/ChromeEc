#!/bin/bash
#
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Calculate hash of fips module

get_hash() {
  local fips_start
  local fips_end
  local size
  local fips_offset
  local offset
  local sections
  local board=${1}
  local elf=build/${board}/RW/ec.RW${2}.elf
  local bin=build/${board}/ec.bin


  offset=0x$( objdump -h ${elf} | awk '/ .text / {print $6}' )

  sections=0x$( objdump -t ${elf} )
  fips_start=0x$( echo "$sections" | awk '/__fips_module_start/ {print $1}' )
  fips_end=0x$( echo "$sections" | awk '/__fips_module_end/ {print $1}' )

  size=$((fips_end - fips_start))
  fips_offset=$((fips_start - offset))
  echo "start: ${fips_start} end:${fips_end} offset:${offset}"
  echo "skip: 0x$(printf '%X' "$((fips_offset))") " \
       "count:0x$(printf '%X' "$((size))")"

  echo "dd if=${bin} skip=${fips_offset} count=${size} bs=1 | sha256sum"
  result=$(dd if=${bin} skip=${fips_offset} count=${size} bs=1 | sha256sum)
  echo "sha256sum: ${result%% *}"
}

main() {
  a=$(get_hash cr50)
  b=$(get_hash cr50 "_B")
  echo "${a}"
  echo "${b}"
  echo "${a}" | awk '/sha256sum: / {print $2}'
  echo "${b}" | awk '/sha256sum: / {print $2}'
}

main

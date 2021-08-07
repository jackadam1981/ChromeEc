#!/bin/bash
#
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Calculate hash of fips module

main() {
  local fips_start
  local fips_end
  local size
  local fips_offset
  local offset
  local sections
  local ro_elf=build/${1}/RO/ec.RO.elf
  local rw_elf=build/${1}/RW/ec.RW${2}.elf
  local ec_bin=build/${1}/ec.bin


  offset=0x$( objdump -h "${ro_elf}" | awk '/ .text / {print $4}' )

  sections=$( objdump -t "${rw_elf}" )
  fips_start=0x$( echo "${sections}" | awk '/__fips_module_start/ {print $1}' )
  fips_end=0x$( echo "${sections}" | awk '/__fips_module_end/ {print $1}' )

  size=$((fips_end - fips_start))
  fips_offset=$((fips_start - offset))

  result=$(dd if="${ec_bin}" skip="${fips_offset}" count="${size}" bs=1 | \
	   sha256sum)
  echo "${result%% *}"
}

main $*

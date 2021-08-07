#!/bin/bash
#
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Calculate hash of fips module

main() {
  local objcopy="${1}"
  local rw_flat="${2}"
  local rw_ext="_B"
  local build_dir
  local offset
  local size
  local sections
  local fips_start
  local fips_end
  local fips_offset
  local result

  if [ "$(basename "${rw_flat}")" = "ec.RW.flat" ] ; then
    rw_ext=""
  fi

  build_dir=$(dirname "${rw_flat}")
  local checksum_section=text.fips_checksum
  local fips_checksum=${build_dir}/fips.checksum${rw_ext}
  local fips_checksum_dump=${fips_checksum}.dump
  local rw_elf=${build_dir}/ec.RW${rw_ext}.elf

  offset=0x$( objdump -h "${rw_elf}" | awk '/ .text / {print $4}' )
  sections=$( objdump -t "${rw_elf}" )

  if [[ "${sections}" =~ "${checksum_section}" ]] ; then
    echo "  get fips checksum"
  else
    echo "  no fips checksum"
    return
  fi
  fips_start=0x$( echo "${sections}" | awk '/__fips_module_start/ {print $1}' )
  fips_end=0x$( echo "${sections}" | awk '/__fips_module_end/ {print $1}' )
  size=$((fips_end - fips_start))
  fips_offset=$((fips_start - offset))

  result=$(dd if="${rw_flat}" skip="${fips_offset}" count="${size}" bs=1 | \
		sha256sum)
  echo "${result%% *}" > "${fips_checksum}"
  echo "${result%% *}" | xxd -r -p  > "${fips_checksum_dump}"

  ${objcopy} --update-section "${checksum_section}"="${fips_checksum_dump}" \
		"${rw_elf}"
}

main "$@"

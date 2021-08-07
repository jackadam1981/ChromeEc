#!/bin/bash
#
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Calculate hash of fips module and inject it into the .elf file.

main() {
  local objcopy="${1}"
  local rw_elf_in="${2}"
  local rw_flat="${3}"
  local base="${rw_elf_in%.elf}"
  local rw_elf_out="${rw_elf_in}.fips"
  local offset
  local size
  local sections
  local fips_start
  local fips_end
  local fips_offset
  local result

  if [ ! -f "${rw_elf_in}" ] ; then
    echo "  ${rw_elf_in} doesn't exist"
    return 1
  fi
  if [ ! -f "${rw_flat}" ] ; then
    echo "  ${rw_flat} doesn't exist"
    return 1
  fi

  local checksum_section="text.fips_checksum"
  local fips_checksum="${base}.fips.checksum"
  local fips_checksum_dump="${fips_checksum}.dump"

  offset=0x$( objdump -h "${rw_elf_in}" | awk '/ .text / {print $4}' )
  sections=$( objdump -t "${rw_elf_in}" )

  if [[ "${sections}" =~ "${checksum_section}" ]] ; then
    echo "  get fips checksum"
  else
    echo "  no fips checksum"
    return 1
  fi

  fips_start=0x$( echo "${sections}" | awk '/__fips_module_start/ {print $1}' )
  fips_end=0x$( echo "${sections}" | awk '/__fips_module_end/ {print $1}' )
  size=$((fips_end - fips_start))
  fips_offset=$((fips_start - offset))

  result=$(dd if="${rw_flat}" skip="${fips_offset}" count="${size}" bs=1 | \
		sha256sum)
  echo "${result%% *}" > "${fips_checksum}"
  echo "${result%% *}" | xxd -r -p  > "${fips_checksum_dump}"

  cp "${rw_elf_in}" "${rw_elf_out}"
  ${objcopy} --update-section "${checksum_section}"="${fips_checksum_dump}" \
		"${rw_elf_out}"
}

main "$@"

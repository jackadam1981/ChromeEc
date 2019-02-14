#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Compares the RO/RW sorted symbol maps (ignoring address) between the current
# build directory and another one specified on the commandline.
#
# usage: compare-smaps.sh other_build_directory

readonly other_build_dir=$1
readonly output_dir="maps"
diff_boards=""

rm -rf "${output_dir}"
mkdir "${output_dir}"

# Given the path to the "smap" file from the build (which is the output of
# "nm" run on the elf file), this function strips off the address (the first
# column), keeping the next two columns (symbol type and symbol name) and then
# sorts them.
#
# Example:
#   input smap contents:
#    081012f8 t gpio_init
#    08101328 T gpio_enable_clocks
#    08101344 T irq_6_handler

#   output:
#    T gpio_enable_clocks
#    t gpio_init
#    T irq_6_handler
sort_excluding_address() {
  cat $1 | cut -f 2-3 -d ' ' | sort
}

compare_smap() {
  local board="$1"
  local type="$2"
  local board_dir="$3"
  local other_board_dir="$4"
  local out_dir="${output_dir}/${board}/${type}"
  if [[ ! -d  "${out_dir}" ]]; then
    mkdir -p "${out_dir}"
  fi
  sorted_file1="${out_dir}/ec.map.cur.txt"
  sorted_file2="${out_dir}/ec.map.other.txt"
  smap_no_address_1="$(sort_excluding_address \
    ${board_dir}/${type}/ec.${type}.smap > ${sorted_file1})"
  smap_no_address_2="$(sort_excluding_address \
    ${other_board_dir}/${type}/ec.${type}.smap > ${sorted_file2})"
  cmp --quiet ${sorted_file1} ${sorted_file2}
  if [[ $? -ne 0 ]]; then
    echo ""
    echo "BOARD: ${board} ${type}: "
    diff "${sorted_file1}" "${sorted_file2}"
    diff_boards="${diff_boards} ${board}_${type}"
  fi
}

for board in `ls build`; do
  if [[ ${board} = "host" ]]; then
    continue
  fi
  board_dir="build/${board}"
  other_board_dir="${other_build_dir}/${board}"
  compare_smap ${board} "RO" ${board_dir} ${other_board_dir}
  compare_smap ${board} "RW" ${board_dir} ${other_board_dir}
done

if [[ -z "${diff_boards}" ]]; then
  exit 0
fi

echo ""
echo "Boards that differ: "
for board in ${diff_boards}; do
  echo "${board}"
done
exit 1

#!/bin/bash
#
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

mapfile -d '' cmakes < <(find zephyr \( -path zephyr/test -prune \) -o \
  -name CMakeLists.txt -print0)

exit_code=0

for file in "$@"; do
  case "${file}" in
    **/platform/ec/test/*) ;;
    **/platform/ec/util/*|**/platform/ec/zephyr/*) ;;
    **/platform/ec/**.c)
      ec_file="${file##**/platform/ec/}"
      if ! grep -q -F "\${PLATFORM_EC}/${ec_file}" "${cmakes[@]}" ; then
        echo -n "WARNING: ${ec_file} is not used in Zephyr EC. Unless you are "
        echo "fixing legacy code only, you should not be editing this file."
        exit_code=1
      fi
      ;;
  esac
done

exit ${exit_code}

#!/bin/bash
#
# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Get the script's absolute directory path
home_dir=$(dirname "$(realpath "$0")")

# Loop until we reach the root directory
while [[ ${home_dir} != "/" ]]; do
  # Check if .repo directory exists
  if [[ -d ${home_dir}/.repo ]]; then
    echo "${home_dir}"
    break
  fi
  home_dir=$(dirname "${home_dir}")  # Go up one level in the directory tree
done

: "${ZEPHYR_BASE:=$(realpath "${home_dir}/src/third_party/zephyr/main")}"
TMP="$(mktemp -d)"
ec_commands_file_out="${TMP}/cros_ec_commands.h"

cleanup() {
  rm -rf "${TMP:?}"
}

trap cleanup EXIT

# Get unifdef from CIPD, and add to PATH
cipd ensure -ensure-file - -root "${TMP:?}" <<EOF
chromiumos/infra/tools/unifdef latest
EOF
export PATH="${TMP:?}/bin:${PATH}"

./util/make_linux_ec_commands_h.sh include/ec_commands.h \
  "${ec_commands_file_out}"

"${ZEPHYR_BASE}/scripts/checkpatch.pl" -f "${ec_commands_file_out}" \
  --ignore=BRACKET_SPACE

cleanup

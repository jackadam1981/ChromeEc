#!/bin/bash
#
# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

: "${ZEPHYR_BASE:=$(realpath ../../../src/third_party/zephyr/main)}"
TMP="$(mktemp -d)"
ec_commands_file_out="${TMP}/cros_ec_commands.h"

cleanup() {
  rm -rf "${TMP:?}"
}

trap cleanup EXIT

echo 'chromiumos/infra/tools/unifdef latest' > "${TMP:?}/ensure_file.txt"
cipd ensure -ensure-file "${TMP:?}/ensure_file.txt" -root "${TMP:?}"
export PATH="${TMP:?}/bin:${PATH}"
./util/make_linux_ec_commands_h.sh include/ec_commands.h \
  "${ec_commands_file_out}"

"${ZEPHYR_BASE}/scripts/checkpatch.pl" -f "${ec_commands_file_out}" \
  --ignore=BRACKET_SPACE

cleanup

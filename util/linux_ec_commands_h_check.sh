#!/bin/bash
#
# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

: "${ZEPHYR_BASE:=$(realpath ../../../src/third_party/zephyr/main)}"
CHROOT_TMP="$(realpath ../../../out/tmp)"

ec_commands_file_out="${CHROOT_TMP}/$$/cros_ec_commands.h"

cleanup() {
  rm -rf "${CHROOT_TMP:?}/$$"
}

trap cleanup EXIT

cros_sdk --working-dir /mnt/host/source/src/platform/ec -- \
  ./util/make_linux_ec_commands_h.sh include/ec_commands.h \
  /tmp/$$/cros_ec_commands.h

"${ZEPHYR_BASE}/scripts/checkpatch.pl" -f "${ec_commands_file_out}" \
  --ignore=BRACKET_SPACE

cleanup

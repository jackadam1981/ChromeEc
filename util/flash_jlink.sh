#!/bin/bash

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

BIN_FILE="$(readlink -f "$1")"
JLINK="./JLink_Linux_V670e_x86_64/JLinkExe"

TMP_FILE="$(mktemp)"

cat <<SETVAR > "${TMP_FILE}"
r
loadfile "${BIN_FILE}"
go
exit
SETVAR

"${JLINK}" -ip 127.0.0.1:2551 -device STM32F412CG -if SWD -speed auto \
  -autoconnect 1 -CommandFile "${TMP_FILE}"

rm -f "${TMP_FILE}"

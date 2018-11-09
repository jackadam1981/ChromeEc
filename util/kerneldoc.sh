#!/bin/sh
#
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Only enforce kernel-doc on the command header, which we often sync with the
# Linux kernel sources.
FILES="include/ec_commands.h"

KERNELDOC="third_party/kernel-doc -none"

TMP="$(mktemp)"
ret=0
for f in ${FILES}
do
	out="$(${KERNELDOC} "${f}" 2>&1)"
	if [ -n "${out}" ]; then
		ret=1
		echo "${out}"
	fi
done
rm "${TMP}"
exit ${ret}

#!/bin/bash
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate a kernel include file from ec_commands.h.
# Syntax
# $0 input output
set -e

ec_commands_file_in="$1"
ec_commands_file_out="$2"

mkdir -p "$(dirname "${ec_commands_file_out}")"
cp "${ec_commands_file_in}" "${ec_commands_file_out}"

# Replace license
patch -p 0 < "util/convert_ec_commands-to_kernel.patch"

# Change header guards
sed -i "s/__CROS_EC_EC_COMMANDS_H/__CROS_EC_COMMANDS_H/" \
  "${ec_commands_file_out}"

# Remove non kernel code to prevent checkpatch warnings and simplify the .h.
unifdef -x2 -m -UCONFIG_HOSTCMD_ALIGNED -U__ACPI__ -D__KERNEL__ -U__cplusplus \
  -UCHROMIUM_EC "${ec_commands_file_out}"

# Check kernel checkpatch passes.
"${CROS_WORKON_SRCROOT}/src/repohooks/checkpatch.pl" \
  -f "${ec_commands_file_out}"

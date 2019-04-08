#!/bin/bash
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate a kernel include file from ec_commands.h.

usage() {
  cat << EOF
Generate an ec_commands.h file suitable for to be upstream to kernel.org.
Syntax:
$0 source_ec_commands.h target_cros_ec_commands.h

source_ec_commands.h: source file, usually include/ec_commands.h
target_cros_ec_commands.h: target file that will be upstreamed.
EOF
}

set -e

ec_commands_file_in="$1"
ec_commands_file_out="$2"

if [ $# -lt 2 ]; then
  usage
  exit 1
fi

ec_commands_file_tmp="$(mktemp -p "$(dirname "${ec_commands_file_out}")" \
  cros_ec_XXX.h)"


cleanup() {
  rm -f "${ec_commands_file_tmp}"*
}

trap cleanup EXIT

mkdir -p "$(dirname "${ec_commands_file_out}")"
cp "${ec_commands_file_in}" "${ec_commands_file_tmp}"

# Replace license
patch ${ec_commands_file_tmp} << EOF
@@ -1,6 +1,11 @@
-/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
- * Use of this source code is governed by a BSD-style license that can be
- * found in the LICENSE file.
+/* SPDX-License-Identifier: GPL-2.0 */
+/*
+ * Host communication command constants for ChromeOS EC
+ *
+ * Copyright (C) 2012 Google, Inc
+ *
+ * NOTE: This file is auto-generated from ChromeOS EC Open Source code from
+ * https://chromium.googlesource.com/chromiumos/platform/ec/+/master/include/ec_commands.h
  */
 
 /* Host communication command constants for Chrome EC */
EOF

# Change header guards
sed -i "s/__CROS_EC_EC_COMMANDS_H/__CROS_EC_COMMANDS_H/" \
  "${ec_commands_file_tmp}"

# Remove non kernel code to prevent checkpatch warnings and simplify the .h.
unifdef -x2 -m -UCONFIG_HOSTCMD_ALIGNED -U__ACPI__ -D__KERNEL__ -U__cplusplus \
  -UCHROMIUM_EC "${ec_commands_file_tmp}"

# Check kernel checkpatch passes.
"${CROS_WORKON_SRCROOT}/src/repohooks/checkpatch.pl" \
  -f "${ec_commands_file_tmp}"

cp "${ec_commands_file_tmp}" "${ec_commands_file_out}"

#!/bin/bash
#
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script converts input binary blob into output .h file,
#
# Input is supposed to be 33 bytes in size, and be a concatenation a 32 bytes
# key and a single byte key ID.
#
# The output .h file contains a C #define statement assigning RMA_KEY_BLOB
# variable to hex dump of the input file.
#
# The two required command line arguments are names of input and output files.
#
# This script is supposed to be invoked from the make file, no command line
# argument verification is done.

# Make sure the user is alerted if not enough command line arguments are
# supplied.
set -u

input_file="${1}"
output_file="${2}"

key_dump="$(od -An -tx1 -w8 ${input_file} | \
    sed 's/^ /\t0x/;s/ /, 0x/g;s/$/, \\/')"

cat > ${output_file} <<EOF
/*
 * This is a generated file, do not edit.
 */

#define RMA_KEY_BLOB { \\
${key_dump}
}

EOF

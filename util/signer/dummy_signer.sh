#!/bin/bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script replaces cr50-codesigner in environments where it is not
# available. The desired output file name is retrieved from the
# --output=<file> command line argument, and then a 1000 bytes of zeros is
# written into that file.
#
# This allows make to proceed, because the next step is treating this
# generated file as an unformatted binary blob

for arg in ${@}; do
    case "${arg}" in
      (--output=*)
	output=${arg#--output=};
        echo "  Generating dummy ${output}"
        dd if=/dev/zero of=$output bs=1 count=1000 >/dev/null  2>&1
        exit 0
        ;;
      (*)
        ;;
    esac
done
echo "$(basename $0): Did not find output file name" >&2
exit 1

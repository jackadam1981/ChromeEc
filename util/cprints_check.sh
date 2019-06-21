#!/bin/bash
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

main() {
  git diff "${PRESUBMIT_COMMIT}~" "${PRESUBMIT_COMMIT}" -U0 | awk '
  /^\+\+\+/ { file=$2; sub(/^b\//, "", file); next }
  /^@@/ { split($3,a,","); line=a[1]; next }
  ! /^\+[^+][^+]/ { line += 1; next }
  /.*(CPRINTS|cprints)\([^\)]*"[^"]*\\n"/ {
    sub(/^+/, "")
    print file ":" line ": Do not add \\n at the end of CPRINTS:"
    print $0
    ret=1
  }
  END { exit(ret) }'
}

main "$@"

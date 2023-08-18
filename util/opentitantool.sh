#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Wrapper script that build opentitan tool with bazel then passes the
# command line parameters to it

set -euo pipefail

main() {
    local script_path
    local opentitan_root
    local bin

    script_path="$(cd $(dirname "${0}")>/dev/null 2>&1 ;  pwd -P)"
    opentitan_root="${script_path}/../../../third_party/lowrisc/opentitan"
    bin="${opentitan_root}/bazel-bin/sw/host/opentitantool/opentitantool"

    # Execute in sub shell so we don't change working directories
    ( "${opentitan_root}/bazelisk.sh" build //sw/host/opentitantool \
        >/dev/null 2>&1 )

    # Call opentitantool from original working directory
    "${bin}" "$@"
}

main "$@"

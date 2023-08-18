#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Wrapper script that build opentitan tool with bazel then passes the
# command line parameters to it

set -euo pipefail

main() {
    local script_path
    local opentitan_path

    pushd "$(dirname "${0}")" >/dev/null 2>&1
    script_path="$(pwd -P)"
    opentitan_root="${script_path}/../../../third_party/lowrisc/opentitan"
    bin="${opentitan_root}/bazel-bin/sw/host/opentitantool/opentitantool"

    pushd "${opentitan_root}" >/dev/null 2>&1
    ./bazelisk.sh build //sw/host/opentitantool  >/dev/null 2>&1
    popd >/dev/null 2>&1
    popd >/dev/null 2>&1

    # Call opentitantool from original working directory
    "${bin}" "$@"
}

main "$@"

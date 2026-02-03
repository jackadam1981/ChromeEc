#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Wrapper script that build opentitan tool with bazel then passes the
# command line parameters to it
# Use FORCE_REBUILD=<anything> to rebuild opentitantool even if binary exists

set -euo pipefail

FORCE_REBUILD="${FORCE_REBUILD:+1}"

main() {
    local script_path
    local opentitan_root
    local bin

    script_path="$(cd "$(dirname \
       "$(test -L "$0" && readlink "$0" || echo "$0")")" >/dev/null 2>&1 ; \
       pwd -P)"
    opentitan_root="${script_path}/../../../third_party/lowrisc/opentitan"
    bin="${opentitan_root}/bazel-bin/sw/host/opentitantool/opentitantool"

    # If we force rebuild or the binary isn't present, build it now
    if [[ -n "${FORCE_REBUILD}" || ! -f "${bin}" ]]; then
      local status=0
      local log

      echo "Rebuilding opentitantool..."
      log=$( "${opentitan_root}/bazelisk.sh" build \
             //sw/host/opentitantool:opentitantool  2>&1 ) || status=$?

      if [[ $status -ne 0 ]]; then
        echo "${script_path} failed to build opentitantool:" >&2
        echo "${log}" >&2
        exit 1
      fi
    fi

    # Call opentitantool from original working directory
    "${bin}" "$@"
}

main "$@"

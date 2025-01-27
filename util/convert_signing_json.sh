#!/bin/bash
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script lets you sanitize the signing json file. It removes the comments
# and extra whitespace from the manifest.

# Two command line parameters are required, the input json and the path
# to store the sanitized file.

set -ue

# Sanatize the manifest
main () {
  local manifest="${1}"
  local output="${2}"

  if [[ -f ${manifest} ]] ; then
    echo "sanitizing ${manifest}"
  else
    echo "${manifest} not found" >&2
    exit 1
  fi
  if [[ -z ${output} ]] ; then
    echo "supply MANIFEST OUTPUT_FILE" >&2
    exit 1
  fi
  echo $(sed 's|\s*//.*$||g; /^\s*$/d' "${manifest}") > "${output}"
}

main "$@"

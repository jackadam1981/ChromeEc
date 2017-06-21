#!/bin/bash
#
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Run all eCTS tests and publish results

set -e

NOC='\033[0m'
RED='\033[0;31m'
GRN='\033[0;32m'
VERBOSITY=""

# List of tests to run.
TESTS=(meta interrupt gpio task timer)

usage() {
  cat <<END

  ${SCRIPT_NAME} - Run all eCTS tests and publish results.
  Usage: ${SCRIPT_NAME} [options]
  Options:
    -d: Dry run tests.
    -h: Print this message
    -r: Only run tests. Skip sync and publishing.
    -v: Enable verbose output

END
}

error() {
  printf "%b[%s] %s%b\n" "${RED}" "${SCRIPT_NAME}" "$@" "${NOC}" 1>&2
}

info() {
  printf "%b[%s] %s%b\n" "${GRN}" "${SCRIPT_NAME}" "$@" "${NOC}"
}

get_script_name() {
  local name=$(basename "$1")
  printf "${name%.*}"
}

get_ec_dir() {
  pushd $(dirname "$1") > /dev/null
  printf "$(pwd)/../.."
  popd > /dev/null
}

is_inside_chroot() {
  [[ -e "/etc/cros_chroot_version" ]]
}

sync_src() {
  info "Syncing tree..."
  if ! repo sync .; then
    error "Failed to sync source"
    exit 1
  fi
}

run_test() {
  cros_sdk -- "${DRY_RUN}" /mnt/host/source/src/platform/ec/cts/cts.py -m "$1"
}

run() {
  local t
  for t in "${TESTS[@]}"; do
    info "Running ${t} test"
    run_test "${t}"
  done
}

upload_results() {
  info "Uploading results... (Not implemented)"
}

main() {
  local run_only

  SCRIPT_NAME=$(get_script_name "$0")
  DRY_RUN=""

  if is_inside_chroot; then
      error "This script must run outside chroot."
      exit 1
  fi

  cd $(get_ec_dir "$0")

  while getopts ":drvh" opt; do
    case "${opt}" in
      d) DRY_RUN="echo" ;;
      h)
        usage
        exit 0
        ;;
      r) run_only=y ;;
      v) VERBOSITY=y ;;
      \?)
        error "invalid option: -${OPTARG}"
        exit 1
        ;;
    esac
  done
  shift $((OPTIND-1))
  
  [[ "${run_only}" == "y" ]] || sync_src
  run
  [[ "${run_only}" == "y" ]] || upload_results
}

main "$@"
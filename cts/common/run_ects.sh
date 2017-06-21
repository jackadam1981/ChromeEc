#!/bin/bash
#
# Run all eCTS tests and publish results

set -e

SCRIPT_NAME=$(basename $0)

# List of tests to run
TESTS="meta interrupt gpio task timer"

usage() {
cat <<END

${SCRIPT_NAME} - Run all eCTS tests and publish results.
Usage: ${SCRIPT_NAME} [options] <chromiumos_dir>
Options:
  -h: Print this message
  -s: Sync tree before running tests
  -v: Enable verbose output

END
}

error() {
  # To avoid conflict with other redirections, use subshell
  (echo -e "[eCTS] $@" >&2)
}

log() {
  (echo -e "[eCTS] $@")
}

get_file_name() {
  local name="${1%.*}"
  printf $name
}

is_inside_chroot() {
  CHROOT_VERSION_FILE=/etc/cros_chroot_version
  [[ -e ${CHROOT_VERSION_FILE} ]]
}

sync_src() {
  log "Syncing tree..."
  cd "${EC_DIR}"
  if ! repo sync .; then
    error "Failed to sync source"
    exit 1
  fi
}

run_test() {
  cros_sdk -- \$HOME/trunk/src/platform/ec/cts/cts.py -m $1
}

run() {
  local t
  for t in ${TESTS}; do
    log "Running ${t} test"
    run_test "${t}"
  done
}

upload_results() {
  log "Uploading results..."
}

setup() {
  log "chromiumos_dir=$1"
  if [[ -z "$1" ]] || [[ ! -e "$1" ]]; then
    error "Chromium OS directory not found"
    usage
    exit 1
  fi
  CHROMIUMOS_DIR="$1"
  EC_DIR="${CHROMIUMOS_DIR}/src/platform/ec"
  cd "${CHROMIUMOS_DIR}"
}

main() {
  [[ "${skip_sync}" == "y" ]] || sync_src
  run
  upload_results
}

### END OF FUNCTIONS ###

verbosity=""
skip_sync="y"

if is_inside_chroot; then
  error "This script must run outside chroot."
  exit 1
fi

while getopts ":svh" opt; do
  case "$opt" in
    h)
      usage
      exit 0
      ;;
    s) skip_sync="" ;;
    v) verbosity=y ;;
    \?)
      error "invalid option: -${OPTARG}"
      exit 1
      ;;
  esac
done
shift $((OPTIND-1))

setup "$1"
shift $((OPTIND-1))

main "$@"
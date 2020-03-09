#!/bin/bash

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool to compare two commits and make sure that the resulting build output is
# exactly the same.

. /usr/share/misc/shflags

DEFINE_string 'board' "nocturne_fp" 'Board to build (\"all\" for all boards)' \
              'b'
DEFINE_string 'ref1' "HEAD" 'Git reference (commit, branch, etc)'
DEFINE_string 'ref2' "HEAD^" 'Git reference (commit, branch, etc)'
DEFINE_string 'keep' "no" 'Keep the temp directory around for inspection.' 'k'
DEFINE_string 'jobs' "" 'Number of jobs to pass to make' 'j'
# When compiling both refs for all boards, mem usage was larger than 32GB.
# If you don't have more than 32GB, you probably don't want to run in parallel.
DEFINE_string 'parallel' "yes" \
                'Make both refs at the same time (extreme mem usage)' 'p'

# Process commandline flags.
FLAGS "${@}" || exit 1
eval set -- "${FLAGS_ARGV}"

set -e

BOARD="${FLAGS_board}"
BOARDS_TO_SKIP="$(grep -E '^skip_boards =' Makefile.rules)"
BOARDS_TO_SKIP="${BOARDS_TO_SKIP//skip_boards = /}"
# Cr50 doesn't have reproducible builds.
# The following fails:
# git commit --allow-empty -m "Test" &&
# ./util/compare_build.sh --board cr50 --ref1 HEAD --ref2 HEAD^
BOARDS_TO_SKIP+=" cr50"

# Can specify any valid git ref (e.g., commits or branches).
# We need the long sha for fetching changes
OLD_REF="$(git rev-parse "${FLAGS_ref1}")"
NEW_REF="$(git rev-parse "${FLAGS_ref2}")"

parse_bool() {
  local value="$1"

  case "${value}" in
    y|yes|true|t|1|on)
      echo yes
      ;;
    n|no|false|f|0|off)
      echo no
      ;;
  esac
}

KEEP_TEMP_DIR="$(parse_bool "${FLAGS_keep}")"
PARALLEL="$(parse_bool "${FLAGS_parallel}")"

MAKE_FLAGS=( )
# Specify -j 1 for sequential
if [[ -n "${FLAGS_jobs}" ]]; then
  MAKE_FLAGS+=( "-j" "${FLAGS_jobs}" )
else
  MAKE_FLAGS+=( "-j" )
fi

BOARDS=( "${BOARD}" )

if [[ "${BOARD}" == "all" ]]; then
  BOARDS=( )
  echo "Skipping boards: ${BOARDS_TO_SKIP}"
  for b in $(make print-boards); do
    skipped=0
    for skip in ${BOARDS_TO_SKIP}; do
      if [[ "${skip}" == "${b}" ]]; then
        skipped=1
        break
      fi
    done
    if [[ ${skipped} == 0 ]]; then
      BOARDS+=( "${b}" )
    fi
  done
fi

echo "BOARDS: ${BOARDS[*]}"

TMP_DIR="$(mktemp -d -t compare_build.XXXX)"

# We want make to initiate the builds for ref1 and ref2 so that a
# single jobserver manages the process. We could do the entire compare
# in make, but let's keep this makefile simple.
echo "# Preparing Makefile"
cat > "${TMP_DIR}/Makefile" <<HEREDOC
ORIGIN ?= $(realpath .)
CRYPTOC_DIR ?= $(realpath ../../third_party/cryptoc)
BOARDS ?= ${BOARDS[*]}

.PHONY: all
all: build-${OLD_REF} build-${NEW_REF}

ec-%:
	git clone \$(ORIGIN) \$@
	git -C \$@ fetch origin \$(@:ec-%=%)
	git -C \$@ checkout --quiet FETCH_HEAD

build-%: ec-%
	\$(MAKE) --no-print-directory -C \$(@:build-%=ec-%)                   \\
		STATIC_VERSION=1                                              \\
		CRYPTOCLIB=\$(CRYPTOC_DIR)                                    \\
		\$(addprefix proj-,\$(BOARDS))
	@printf "  MKDIR   %s\n" "\$@"
	@mkdir -p \$@
	@for b in \$(BOARDS); do	                                      \\
		printf "  CP -l   '%s' to '%s'\n"                             \\
			"\$(@:build-%=ec-%)/build/\$\$b/ec.bin"               \\
                        "\$@/\$\$b-ec.bin";                                   \\
		cp -l \$(@:build-%=ec-%)/build/\$\$b/ec.bin \$@/\$\$b-ec.bin; \\
	done

# So that make doesn't try to remove them
ec-${OLD_REF}:
ec-${NEW_REF}:
HEREDOC


build() {
  echo make --no-print-directory -C "${TMP_DIR}" "${MAKE_FLAGS[@]}"  "$@"
  make --no-print-directory -C "${TMP_DIR}" "${MAKE_FLAGS[@]}"  "$@"
  return $?
}

echo "# Launching build. Cover your eyes."
result=1
if [[ "${PARALLEL}" == "yes" ]]; then
  build "build-${OLD_REF}" "build-${NEW_REF}"
  result=$?
else
  build "build-${OLD_REF}" && build "build-${NEW_REF}"
  result=$?
fi
if [[ ${result} -ne 0 ]]; then
  echo >&2
  echo "# Failed to make one or more of the refs." >&2
  exit 1
fi
echo

echo "# Comparing Files"
echo
if diff "${TMP_DIR}/build-"{"${OLD_REF}","${NEW_REF}"}; then
  echo "# ec.bin MATCH"
  result=0
else
  echo "# ec.bin FAILURE"
  result=1
fi
echo

# Do keep in mind that temp directory take a few GB if all boards are built.
if [[ "${KEEP_TEMP_DIR}" == "no" ]]; then
  echo "# Removing temp directory"
  rm -rf "${TMP_DIR}"
else
  echo "# Keeping temp directory around for your inspection."
  echo "# ${TMP_DIR}"
fi

exit "${result}"

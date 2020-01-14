#!/bin/bash

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool to compare two commits and make sure that the resulting build output is
# exactly the same.

. /usr/share/misc/shflags

DEFINE_string 'board' "nocturne_fp" 'Board to build' 'b'
DEFINE_string 'ref1' "HEAD" 'Git reference (commit, branch, etc)'
DEFINE_string 'ref2' "HEAD^" 'Git reference (commit, branch, etc)'

# Process commandline flags
FLAGS "${@}" || exit 1
eval set -- "${FLAGS_ARGV}"

BOARD="${FLAGS_board}"

# Can specify any valid git ref (e.g., commits or branches)
OLD_REF="$(git rev-parse --short "${FLAGS_ref1}")"
NEW_REF="$(git rev-parse --short "${FLAGS_ref2}")"

SAVED_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

OLD_BUILD_DIR="build.${BOARD}_${OLD_REF}"
NEW_BUILD_DIR="build.${BOARD}_${NEW_REF}"

# Make sure the generated ec_version.h is constant. See util/getversion.sh.
export STATIC_VERSION=1

do_build() {
	local ref="$1"
	local result_dir="$2"
	local board="$3"
	git checkout "${ref}"
	echo "Testing commit: $(git rev-parse --short HEAD)"
	rm -rf "build/${board}" && make V=1 BOARD="${board}" -j > ./build.out
	rm -rf "${result_dir}"
	mv "build/${board}" "${result_dir}"
	mv build.out "${result_dir}"
}

do_build "${OLD_REF}" "${OLD_BUILD_DIR}" "${BOARD}"
do_build "${NEW_REF}" "${NEW_BUILD_DIR}" "${BOARD}"

git checkout "${SAVED_BRANCH}"

if ! diff "${OLD_BUILD_DIR}/ec.bin" "${NEW_BUILD_DIR}/ec.bin"; then
	echo "ec.bin FAILURE"
	exit 1
fi

echo "ec.bin MATCH"

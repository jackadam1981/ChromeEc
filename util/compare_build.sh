#!/bin/bash

# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool to compare two commits and make sure that the resulting build output is
# exactly the same.
#
# Usage: compare_build.sh <BOARD> <OLD_REF> <NEW_REF>

BOARD="$1"

# Can specify any valid git ref (e.g., commits or branches)
OLD_REF="$(git rev-parse --short "$2")"
NEW_REF="$(git rev-parse --short "$3")"

SAVED_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

OLD_BUILD_DIR="build.${BOARD}_${OLD_REF}"
NEW_BUILD_DIR="build.${BOARD}_${NEW_REF}"

export EC_CUSTOM_VERSION="test_version"
export EC_CUSTOM_DATE="now"

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
